#include "world.h"

#include <cmath>
#include <algorithm>
#include <cstdio>

World::World(const Config& cfg) : cfg_(cfg) {}

bool World::in_world_bounds(const ChunkPos& p) const {
    // Y de 0 a 11 (12 chunks). X e Z: o proprio int16_t ja limita.
    return p.y >= 0 && p.y < world_const::CHUNKS_Y;
}

Chunk* World::get_chunk(const ChunkPos& p) {
    auto it = chunks_.find(p);
    return it == chunks_.end() ? nullptr : &it->second;
}

const Chunk* World::get_chunk(const ChunkPos& p) const {
    auto it = chunks_.find(p);
    return it == chunks_.end() ? nullptr : &it->second;
}

BlockID World::get_block(int wx, int wy, int wz) const {
    if (wy < 0 || wy >= world_const::WORLD_HEIGHT) return block_id::AR;
    ChunkPos cp = world_to_chunk(wx, wy, wz);
    if (!in_world_bounds(cp)) return block_id::AR;
    auto it = chunks_.find(cp);
    if (it == chunks_.end()) return block_id::AR;
    LocalPos lp = world_to_local(wx, wy, wz);
    return (BlockID)it->second.get(lp.x, lp.y, lp.z);
}

void World::set_block(int wx, int wy, int wz, BlockID id) {
    if (wy < 0 || wy >= world_const::WORLD_HEIGHT) return;
    ChunkPos cp = world_to_chunk(wx, wy, wz);
    if (!in_world_bounds(cp)) return;
    auto it = chunks_.find(cp);
    if (it == chunks_.end()) return; // nao carrega por causa de uma escrita solta
    LocalPos lp = world_to_local(wx, wy, wz);
    it->second.set(lp.x, lp.y, lp.z, id);
    it->second.dirty     = true;
    it->second.needsMesh = true;

    // Se o bloco esta na borda do chunk, os vizinhos precisam remeshar
    // (as faces visiveis mudam). A luz tambem pode atravessar; a parte de
    // iluminacao cuida disso depois. Aqui so marcamos needsMesh dos vizinhos
    // que estao na borda.
    auto mark = [&](int dx, int dy, int dz) {
        ChunkPos n{ (int16_t)(cp.x + dx), (int16_t)(cp.y + dy), (int16_t)(cp.z + dz) };
        auto nit = chunks_.find(n);
        if (nit != chunks_.end()) nit->second.needsMesh = true;
    };
    if (lp.x == 0)                   mark(-1, 0, 0);
    if (lp.x == world_const::CHUNK_SIZE - 1) mark( 1, 0, 0);
    if (lp.y == 0)                   mark( 0,-1, 0);
    if (lp.y == world_const::CHUNK_SIZE - 1) mark( 0, 1, 0);
    if (lp.z == 0)                   mark( 0, 0,-1);
    if (lp.z == world_const::CHUNK_SIZE - 1) mark( 0, 0, 1);
}

Chunk& World::ensure_chunk(const ChunkPos& p) {
    auto it = chunks_.find(p);
    if (it != chunks_.end()) return it->second;
    Chunk& c = chunks_[p];
    c.pos = p;
    return c;
}

void World::update_loaded_region(float px, float py, float pz,
                                 std::vector<ChunkPos>& out_newly_loaded,
                                 std::vector<ChunkPos>& out_unloaded) {
    out_newly_loaded.clear();
    out_unloaded.clear();

    const int r       = cfg_.chunk_radius;
    const int r2      = r * r;
    const int unload_r  = r + 1;
    const int unload_r2 = unload_r * unload_r;

    const int pcx = floor_div((int)std::floor(px), world_const::CHUNK_SIZE);
    const int pcy = floor_div((int)std::floor(py), world_const::CHUNK_SIZE);
    const int pcz = floor_div((int)std::floor(pz), world_const::CHUNK_SIZE);

    // --- Carregamento: esfera 3D, do mais proximo ao mais distante ---
    // Coleta os candidatos dentro da esfera, ordena por distancia^2 e so
    // entao insere. O numero de candidatos e modesto (raio 5 -> ~500 chunks),
    // entao o custo e baixo. A ordenacao importa para a Parte 3: o que esta
    // perto do jogador aparece na tela primeiro.
    std::vector<ChunkPos> candidates;
    candidates.reserve((2 * r + 1) * (2 * r + 1) * (2 * r + 1));
    for (int dy = -r; dy <= r; ++dy) {
        for (int dz = -r; dz <= r; ++dz) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy + dz * dz > r2) continue;
                ChunkPos cp{ (int16_t)(pcx + dx), (int16_t)(pcy + dy),
                             (int16_t)(pcz + dz) };
                if (!in_world_bounds(cp)) continue;
                if (chunks_.find(cp) != chunks_.end()) continue;
                candidates.push_back(cp);
            }
        }
    }
    
    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(),
            [pcx, pcy, pcz](const ChunkPos& a, const ChunkPos& b) {
                int ax = a.x - pcx, ay = a.y - pcy, az = a.z - pcz;
                int bx = b.x - pcx, by = b.y - pcy, bz = b.z - pcz;
                return (ax*ax + ay*ay + az*az) < (bx*bx + by*by + bz*bz);
            });
        

        for (const auto& cp : candidates) {
            Chunk& c = chunks_[cp];
            c.pos = cp;
            out_newly_loaded.push_back(cp);
        }
    }

    // --- Descarregamento com histerese 3D ---
    std::vector<ChunkPos> to_remove;
    to_remove.reserve(chunks_.size() / 4 + 1);
    for (const auto& kv : chunks_) {
        const ChunkPos& cp = kv.first;
        int dx = (int)cp.x - pcx;
        int dy = (int)cp.y - pcy;
        int dz = (int)cp.z - pcz;
        if (dx * dx + dy * dy + dz * dz > unload_r2) to_remove.push_back(cp);
    }
    for (const auto& cp : to_remove) {
        // save dos dirty entra na semana 4
        chunks_.erase(cp);
        out_unloaded.push_back(cp);
    }
}