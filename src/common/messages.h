#pragma once

#include "types.h"
#include "blocks.h"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Vertice compacto do mesh de chunk (secao 5.2 do documento), 8 bytes.
//
//   pos:      coordenada LOCAL dentro do chunk, 0..32 (cabe em uint8).
//   face_uv:  byte empacotado:
//               bits 0-1: u (0..1)
//               bits 2-3: v (0..1)
//               bits 4-6: face (0..5)
//               bit  7:   livre
//   layer:    camada do GL_TEXTURE_2D_ARRAY.
//   pad:      alinhamento a 4 bytes (futuro AO por vertice).
//   light:    RGB + ceu, empacotado como o array de luz do chunk.
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct Vertex {
    std::uint8_t pos_x = 0;
    std::uint8_t pos_y = 0;
    std::uint8_t pos_z = 0;
    std::uint8_t face_uv = 0;
    std::uint8_t layer = 0;
    std::uint8_t pad = 0;
    std::uint16_t light = 0;
};
#pragma pack(pop)

static_assert(sizeof(Vertex) == 8, "Vertex deve ter exatamente 8 bytes");

namespace face_index {
constexpr int TOPO   = 0;
constexpr int BASE   = 1;
constexpr int LESTE  = 2;
constexpr int OESTE  = 3;
constexpr int SUL    = 4;
constexpr int NORTE  = 5;
} // namespace face_index

inline std::uint8_t pack_face_uv(int face, int u, int v) {
    return (std::uint8_t)(((face & 0x7) << 4) | ((v & 0x3) << 2) | (u & 0x3));
}
inline int unpack_face(std::uint8_t fu) { return (fu >> 4) & 0x7; }
inline int unpack_u(std::uint8_t fu)    { return fu & 0x3; }
inline int unpack_v(std::uint8_t fu)    { return (fu >> 2) & 0x3; }

inline std::uint16_t pack_light(int r, int g, int b, int sky) {
    return (std::uint16_t)((r & 0xF) | ((g & 0xF) << 4) |
                           ((b & 0xF) << 8) | ((sky & 0xF) << 12));
}

// ---------------------------------------------------------------------------
// Mensagens
// ---------------------------------------------------------------------------

struct InputState {
    float move_forward = 0.0f;
    float move_right   = 0.0f;
    float move_up      = 0.0f;
    float mouse_dx = 0.0f;
    float mouse_dy = 0.0f;
    bool shift_held = false;
    bool break_pressed = false;
    bool place_pressed = false;
    BlockID selected_block = 1;
    bool  time_pause_toggle = false;
    float time_speed_delta  = 0.0f;
};

struct PlayerSnapshot {
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float yaw   = 0.0f;
    float pitch = 0.0f;
    float time_of_day = 0.0f;
    bool has_target    = false;
    int  target_x = 0, target_y = 0, target_z = 0;
    int  target_face = -1;
    std::int32_t chunk_x = 0;
    std::int32_t chunk_y = 0;
    std::int32_t chunk_z = 0;
    std::int32_t chunks_loaded = 0;
    const char* biome_name = "?";
    BlockID selected_block = 1;
};

struct ChunkMeshUpdate {
    ChunkPos pos{};
    std::vector<Vertex>        opaque_vertices;
    std::vector<std::uint32_t> opaque_indices;
    std::vector<Vertex>        transparent_vertices;
    std::vector<std::uint32_t> transparent_indices;
};

struct ChunkUnload {
    ChunkPos pos{};
};