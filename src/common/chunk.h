#pragma once

#include "types.h"
#include "blocks.h"

#include <cstdint>
#include <memory>

// Chunk: 32x32x32 blocos (32 KB) + luz (64 KB), ambos na heap.
// Propriedade: somente a thread do backend le e escreve estes dados.

struct Chunk {
    ChunkPos pos{};

    // Blocos: CHUNK_VOLUME bytes. Alocado na construcao.
    std::unique_ptr<std::uint8_t[]> blocks;

    // Luz RGB + ceu, 4 bits por canal. Preenchido na Parte de iluminacao.
    // Deixado zerado por enquanto.
    std::unique_ptr<std::uint16_t[]> light;

    // Flags de controle.
    bool dirty     = false;  // modificado pelo jogador (precisa salvar)
    bool needsMesh = true;   // mesh desatualizado (precisa remeshar)
    bool generated = false;  // se ja passou pela geracao

    Chunk() {
        blocks = std::make_unique<std::uint8_t[]>(world_const::CHUNK_VOLUME);
        light  = std::make_unique<std::uint16_t[]>(world_const::CHUNK_VOLUME);
        // Zera blocos (ar) e luz.
        for (int i = 0; i < world_const::CHUNK_VOLUME; ++i) {
            blocks[i] = block_id::AR;
            light[i]  = 0;
        }
    }

    // Acesso local (sem checagem de limite).
    inline std::uint8_t get(int lx, int ly, int lz) const {
        return blocks[local_index(lx, ly, lz)];
    }
    inline void set(int lx, int ly, int lz, std::uint8_t b) {
        blocks[local_index(lx, ly, lz)] = b;
    }

    inline std::uint16_t get_light(int lx, int ly, int lz) const {
        return light[local_index(lx, ly, lz)];
    }
    inline void set_light(int lx, int ly, int lz, std::uint16_t v) {
        light[local_index(lx, ly, lz)] = v;
    }
};