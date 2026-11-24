#pragma once

#include "config.h"
#include "world.h"
#include "player.h"
#include "messages.h"
#include "queue.h"
#include "generator.h"
#include "mesher.h"

#include <atomic>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Backend: roda numa thread propria a ~60 ticks/s.
// Dono do World, do Player, do tempo do dia, do Generator e do Mesher.
// Comunica-se com o client apenas via filas e snapshot protegido por mutex.
// ---------------------------------------------------------------------------
class Backend {
public:
    Backend();
    ~Backend();

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    bool init(const char* config_path);

    void start();
    void stop();

    // --- Comunicacao com o client ---
    Queue<InputState>&      input_queue()    { return input_queue_; }
    Queue<ChunkMeshUpdate>& mesh_queue()     { return mesh_queue_; }
    Queue<ChunkUnload>&     unload_queue()   { return unload_queue_; }
    PlayerSnapshot          latest_snapshot() const;

private:
    void run();
    void tick(float dt);
    void handle_input(const InputState& in, float dt);
    void publish_snapshot();

    // Geracao e meshing.
    void process_generation(int budget);
    void collect_mesh_queue_from_world();
    void process_meshing(int budget);

    // Verdadeiro se o chunk e todos os vizinhos DE FACE *que existem no
    // mundo carregado* ja foram gerados. Vizinhos fora do mundo carregado
    // nao contam (sao tratados como ar pelo mesher).
    bool chunk_and_neighbors_generated(const ChunkPos& cp) const;

    // Marca needsMesh nos vizinhos de face que ja foram gerados. Chamado
    // quando um chunk termina de gerar.
    void mark_neighbors_needs_mesh(const ChunkPos& cp);

    Config  cfg_;
    World   world_;
    Player  player_;
    Generator generator_;
    Mesher    mesher_;

    Queue<InputState>      input_queue_;
    Queue<ChunkMeshUpdate> mesh_queue_;
    Queue<ChunkUnload>     unload_queue_;

    mutable std::mutex snapshot_mutex_;
    PlayerSnapshot     last_snapshot_{};

    float time_of_day_  = 0.35f;
    float day_duration_ = 240.0f;
    float time_scale_   = 1.0f;
    bool  time_paused_  = false;

    std::thread       thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Regiao carregada.
    ChunkPos last_player_chunk_{ std::numeric_limits<std::int16_t>::min(),
                                 std::numeric_limits<std::int16_t>::min(),
                                 std::numeric_limits<std::int16_t>::min() };
    bool region_needs_update_ = true;

    // Filas internas de trabalho.
    std::deque<ChunkPos> gen_queue_;
    std::deque<ChunkPos> mesh_queue_internal_;
    std::unordered_set<ChunkPos, ChunkPosHash> mesh_pending_;
};