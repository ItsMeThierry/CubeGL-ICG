#include "backend.h"
#include "blocks.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

using Clock = std::chrono::steady_clock;

static constexpr float TICK_DT     = 1.0f / 60.0f;
static constexpr auto  TICK_PERIOD = std::chrono::microseconds(16667);

static constexpr int GEN_BUDGET_PER_TICK  = 2;
static constexpr int MESH_BUDGET_PER_TICK = 4;

Backend::Backend() : world_(cfg_) {}
Backend::~Backend() { stop(); }

bool Backend::init(const char* config_path) {
    blocks_init();
    if (!config_load(config_path, cfg_)) {
        std::fprintf(stderr, "[backend] usando config padrao\n");
    }
    world_ = World(cfg_);

    player_.speed       = cfg_.player_speed;
    player_.shift_mult  = cfg_.player_shift_mult;
    player_.sensitivity = cfg_.player_sensitivity;
    player_.yaw         = -90.0f;
    player_.pitch       = 0.0f;

    generator_.init(cfg_);
    const int h = generator_.surface_height_at(0, 0);
    player_.px = 0.0f;
    player_.pz = 0.0f;
    player_.py = (float)(h + 10);

    time_of_day_  = cfg_.day_initial_t;
    day_duration_ = cfg_.day_duration_s;

    publish_snapshot();

    return true;
}

void Backend::start() {
    if (running_.exchange(true)) return;
    stop_requested_.store(false);
    thread_ = std::thread(&Backend::run, this);
}

void Backend::stop() {
    if (!running_.exchange(false)) return;
    stop_requested_.store(true);
    input_queue_.shutdown();
    mesh_queue_.shutdown();
    unload_queue_.shutdown();
    if (thread_.joinable()) thread_.join();
}

void Backend::run() {
    auto next = Clock::now();
    while (!stop_requested_.load()) {
        next += TICK_PERIOD;
        auto now = Clock::now();
        if (now < next) {
            std::this_thread::sleep_until(next);
        } else if (now - next > std::chrono::milliseconds(50)) {
            next = now;
        }
        tick(TICK_DT);
    }
}

void Backend::tick(float dt) {
    // 1. Input.
    InputState agg{};
    InputState one;
    while (input_queue_.try_pop(one)) {
        agg.move_forward += one.move_forward;
        agg.move_right   += one.move_right;
        agg.move_up      += one.move_up;
        agg.mouse_dx     += one.mouse_dx;
        agg.mouse_dy     += one.mouse_dy;
        agg.shift_held   = agg.shift_held || one.shift_held;
        agg.break_pressed = agg.break_pressed || one.break_pressed;
        agg.place_pressed = agg.place_pressed || one.place_pressed;
        agg.selected_block = one.selected_block;
        agg.time_pause_toggle = agg.time_pause_toggle || one.time_pause_toggle;
        agg.time_speed_delta += one.time_speed_delta;
    }
    handle_input(agg, dt);

    // 2. Tempo do dia.
    if (!time_paused_) {
        float adv = (dt / day_duration_) * time_scale_;
        time_of_day_ += adv;
        if (time_of_day_ >= 1.0f) time_of_day_ -= 1.0f;
        if (time_of_day_ <  0.0f) time_of_day_ += 1.0f;
    }

    // 3. Regiao carregada (so ao cruzar chunk).
    ChunkPos cur_chunk = world_to_chunk(
        (int)std::floor(player_.px),
        (int)std::floor(player_.py),
        (int)std::floor(player_.pz));

    if (region_needs_update_ || !(cur_chunk == last_player_chunk_)) {
        std::vector<ChunkPos> newly, unloaded;
        world_.update_loaded_region(player_.px, player_.py, player_.pz,
                                    newly, unloaded);

        for (const auto& cp : unloaded) {
            mesh_pending_.erase(cp);
            ChunkUnload u; u.pos = cp;
            unload_queue_.push(u);
        }
        for (const auto& cp : newly) {
            gen_queue_.push_back(cp);
        }
        last_player_chunk_   = cur_chunk;
        region_needs_update_ = false;
    }

    // 4a. Geracao.
    process_generation(GEN_BUDGET_PER_TICK);

    // 4b. Meshing.
    collect_mesh_queue_from_world();
    process_meshing(MESH_BUDGET_PER_TICK);

    // 5. Snapshot.
    publish_snapshot();
}

void Backend::handle_input(const InputState& in, float dt) {
    InputState mv = in;
    float len = std::sqrt(mv.move_forward * mv.move_forward +
                          mv.move_right   * mv.move_right   +
                          mv.move_up      * mv.move_up);
    if (len > 1.0f) {
        mv.move_forward /= len;
        mv.move_right   /= len;
        mv.move_up      /= len;
    }

    player_.update(mv, dt);

    using namespace world_const;
    if (player_.px < WORLD_XZ_MIN) player_.px = WORLD_XZ_MIN;
    if (player_.px > WORLD_XZ_MAX) player_.px = WORLD_XZ_MAX;
    if (player_.pz < WORLD_XZ_MIN) player_.pz = WORLD_XZ_MIN;
    if (player_.pz > WORLD_XZ_MAX) player_.pz = WORLD_XZ_MAX;
    if (player_.py < PLAYER_Y_MIN) player_.py = PLAYER_Y_MIN;
    if (player_.py > PLAYER_Y_MAX) player_.py = PLAYER_Y_MAX;

    if (in.time_pause_toggle) time_paused_ = !time_paused_;
    if (in.time_speed_delta > 0.0f) {
        // Cicla velocidades uteis para a demo: 1x -> 4x -> 16x -> 64x -> 1x.
        if      (time_scale_ <  4.0f) time_scale_ =  4.0f;
        else if (time_scale_ < 16.0f) time_scale_ = 16.0f;
        else if (time_scale_ < 64.0f) time_scale_ = 64.0f;
        else                          time_scale_ =  1.0f;
    }

    (void)in.break_pressed;
    (void)in.place_pressed;
}

// ---------------------------------------------------------------------------
// Geracao
// ---------------------------------------------------------------------------

void Backend::process_generation(int budget) {
    int done = 0;
    while (done < budget && !gen_queue_.empty()) {
        ChunkPos cp = gen_queue_.front();
        gen_queue_.pop_front();

        Chunk* c = world_.get_chunk(cp);
        if (!c) continue;
        if (c->generated) continue;

        generator_.generate_chunk(cp, *c);
        c->generated = true;
        c->needsMesh = true;

        // Vizinhos ja gerados precisam remeshar: antes deste chunk gerar,
        // as faces deles contra este lado foram omitidas (vizinho vazio).
        mark_neighbors_needs_mesh(cp);

        ++done;
    }
}

void Backend::mark_neighbors_needs_mesh(const ChunkPos& cp) {
    const int dx[6] = { -1,  1,  0,  0,  0,  0 };
    const int dy[6] = {  0,  0, -1,  1,  0,  0 };
    const int dz[6] = {  0,  0,  0,  0, -1,  1 };
    for (int i = 0; i < 6; ++i) {
        ChunkPos n{ (std::int16_t)(cp.x + dx[i]),
                    (std::int16_t)(cp.y + dy[i]),
                    (std::int16_t)(cp.z + dz[i]) };
        if (n.y < 0 || n.y >= world_const::CHUNKS_Y) continue;
        Chunk* c = world_.get_chunk(n);
        if (c && c->generated) c->needsMesh = true;
    }
}

// ---------------------------------------------------------------------------
// Meshing
// ---------------------------------------------------------------------------

bool Backend::chunk_and_neighbors_generated(const ChunkPos& cp) const {
    const Chunk* self = world_.get_chunk(cp);
    if (!self || !self->generated) return false;

    // Vizinho "existe no mundo carregado" = esta em chunks_.
    // Se nao existe (borda da esfera), o mesher trata como ar e mesha agora.
    // Se existe mas nao foi gerado, espera.
    const int dx[6] = { -1,  1,  0,  0,  0,  0 };
    const int dy[6] = {  0,  0, -1,  1,  0,  0 };
    const int dz[6] = {  0,  0,  0,  0, -1,  1 };
    for (int i = 0; i < 6; ++i) {
        ChunkPos n{ (std::int16_t)(cp.x + dx[i]),
                    (std::int16_t)(cp.y + dy[i]),
                    (std::int16_t)(cp.z + dz[i]) };
        if (n.y < 0 || n.y >= world_const::CHUNKS_Y) continue; // fora do mundo
        const Chunk* c = world_.get_chunk(n);
        if (!c) continue;            // nao carregado: trata como ar
        if (!c->generated) return false; // carregado mas nao gerado: espera
    }
    return true;
}

void Backend::collect_mesh_queue_from_world() {
    for (auto& kv : world_.chunks_mutable()) {
        Chunk& c = kv.second;
        if (!c.needsMesh) continue;
        if (mesh_pending_.insert(c.pos).second) {
            mesh_queue_internal_.push_back(c.pos);
        }
    }
}

void Backend::process_meshing(int budget) {
    // Estrategia: percorre a fila INTEIRA neste tick. Os chunks que podem ser
    // meshados agora sao processados (ate o orcamento); os que estao
    // esperando vizinhos sao RE-ENFILEIRADOS SEM CONSUMIR ORCAMENTO. Assim,
    // um chunk de borda preso nao bloqueia os internos.
    //
    // O orcamento limita o numero de meshes FEITOS por tick. Se o orcamento
    // acaba no meio, o resto (que pode) fica para o proximo tick.
    const int total = (int)mesh_queue_internal_.size();
    int done = 0;

    for (int i = 0; i < total; ++i) {
        ChunkPos cp = mesh_queue_internal_.front();
        mesh_queue_internal_.pop_front();

        Chunk* c = world_.get_chunk(cp);
        if (!c || !c->needsMesh) {
            mesh_pending_.erase(cp);
            continue;
        }

        if (done >= budget) {
            // Orcamento esgotado: devolve para a fila e para o loop.
            mesh_queue_internal_.push_back(cp);
            continue;
        }

        if (!chunk_and_neighbors_generated(cp)) {
            // Espera vizinhos: re-enfileira SEM consumir orcamento.
            mesh_queue_internal_.push_back(cp);
            continue;
        }

        MeshData md = mesher_.build_chunk_mesh(world_, cp);
        c->needsMesh = false;
        mesh_pending_.erase(cp);

        ChunkMeshUpdate up;
        up.pos = cp;
        up.opaque_vertices      = std::move(md.opaque_vertices);
        up.opaque_indices       = std::move(md.opaque_indices);
        up.transparent_vertices = std::move(md.transparent_vertices);
        up.transparent_indices  = std::move(md.transparent_indices);
        mesh_queue_.push(std::move(up));

        ++done;
    }
}

// ---------------------------------------------------------------------------
// Snapshot
// ---------------------------------------------------------------------------

void Backend::publish_snapshot() {
    PlayerSnapshot s;
    player_.fill_snapshot(s);
    s.time_of_day   = time_of_day_;
    s.selected_block = 1;

    s.chunk_x = (std::int32_t)floor_div((int)std::floor(player_.px),
                                        world_const::CHUNK_SIZE);
    s.chunk_y = (std::int32_t)floor_div((int)std::floor(player_.py),
                                        world_const::CHUNK_SIZE);
    s.chunk_z = (std::int32_t)floor_div((int)std::floor(player_.pz),
                                        world_const::CHUNK_SIZE);
    s.chunks_loaded = (std::int32_t)world_.chunks().size();
    s.biome_name = "?";

    std::lock_guard<std::mutex> lk(snapshot_mutex_);
    last_snapshot_ = s;
}

PlayerSnapshot Backend::latest_snapshot() const {
    std::lock_guard<std::mutex> lk(snapshot_mutex_);
    return last_snapshot_;
}