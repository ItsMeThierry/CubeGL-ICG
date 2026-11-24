#pragma once

#include <cstdint>

// Dimensoes do mundo
namespace world_const {

constexpr int CHUNK_SIZE   = 32;                 // blocos por eixo do chunk
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; // 32768

constexpr int WORLD_HEIGHT = 384;                // Y de 0 a 383
constexpr int CHUNKS_Y     = WORLD_HEIGHT / CHUNK_SIZE; // 12

// Limites seguros para a posicao do jogador (evita overflow em int16_t).
constexpr int WORLD_XZ_MIN = -32768;
constexpr int WORLD_XZ_MAX =  32767;
constexpr int PLAYER_Y_MIN = -64;
constexpr int PLAYER_Y_MAX =  448;

} // namespace world_const

// Coordenada de chunk: X e Z cabem em int16_t (2048 chunks por eixo);
// Y cabe em int8_t (12 chunks na vertical), mas uso int16_t por simplicidade.
struct ChunkPos {
    int16_t x = 0;
    int16_t y = 0;
    int16_t z = 0;

    bool operator==(const ChunkPos& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct ChunkPosHash {
    std::size_t operator()(const ChunkPos& p) const noexcept {
        // Empacota em 64 bits: 16 bits por eixo. Como cada eixo e int16_t,
        // o cast para uint16_t preserva o padrao de bits.
        std::uint64_t k =
            (std::uint64_t)(std::uint16_t)p.x |
            ((std::uint64_t)(std::uint16_t)p.y << 16) |
            ((std::uint64_t)(std::uint16_t)p.z << 32);
        // Mistura simples (splitmix64) para dispersar bem.
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        return (std::size_t)k;
    }
};

// Para eixos negativos, o "floor division" e o que queremos:
// bloco -1 -> chunk -1, local 31. O C++ faz truncamento, entao ajustamos.
inline int floor_div(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

inline int floor_mod(int a, int b) {
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

inline ChunkPos world_to_chunk(int wx, int wy, int wz) {
    ChunkPos p;
    p.x = (int16_t)floor_div(wx, world_const::CHUNK_SIZE);
    p.y = (int16_t)floor_div(wy, world_const::CHUNK_SIZE);
    p.z = (int16_t)floor_div(wz, world_const::CHUNK_SIZE);
    return p;
}

// Indice local dentro do chunk: 0..31 em cada eixo.
struct LocalPos {
    int x = 0;
    int y = 0;
    int z = 0;
};

inline LocalPos world_to_local(int wx, int wy, int wz) {
    LocalPos p;
    p.x = floor_mod(wx, world_const::CHUNK_SIZE);
    p.y = floor_mod(wy, world_const::CHUNK_SIZE);
    p.z = floor_mod(wz, world_const::CHUNK_SIZE);
    return p;
}

// Indice linear no array do chunk. Ordem: x + z*32 + y*32*32.
inline int local_index(int lx, int ly, int lz) {
    return lx + lz * world_const::CHUNK_SIZE +
           ly * world_const::CHUNK_SIZE * world_const::CHUNK_SIZE;
}

// Coordenada de mundo a partir de chunk + local.
inline void chunk_local_to_world(const ChunkPos& c, int lx, int ly, int lz,
                                 int& wx, int& wy, int& wz) {
    wx = (int)c.x * world_const::CHUNK_SIZE + lx;
    wy = (int)c.y * world_const::CHUNK_SIZE + ly;
    wz = (int)c.z * world_const::CHUNK_SIZE + lz;
}