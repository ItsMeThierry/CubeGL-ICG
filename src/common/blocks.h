#pragma once

#include <cstdint>

// Registro de blocos
// Tabela somente leitura depois da inicializacao; compartilhada pelas threads.
using BlockID = std::uint8_t;

namespace block_id {
constexpr BlockID AR            = 0;
constexpr BlockID PEDRA         = 1;
constexpr BlockID TERRA         = 2;
constexpr BlockID GRAMA         = 3;
constexpr BlockID AREIA         = 4;
constexpr BlockID NEVE          = 5;
constexpr BlockID AGUA          = 6;
constexpr BlockID LANTERNA_VERM = 7;
constexpr BlockID LANTERNA_AZUL = 8;
constexpr BlockID LANTERNA_VERD = 9;
constexpr BlockID VIDRO         = 10;
constexpr int     COUNT         = 11;
} // namespace block_id

// Camadas do texture array
namespace tex_layer {
constexpr int PEDRA         = 0;
constexpr int TERRA         = 1;
constexpr int GRAMA_TOPO    = 2;
constexpr int GRAMA_LADO    = 3;
constexpr int AREIA         = 4;
constexpr int NEVE          = 5;
constexpr int AGUA          = 6;
constexpr int LANTERNA_VERM = 7;
constexpr int LANTERNA_AZUL = 8;
constexpr int LANTERNA_VERD = 9;
constexpr int VIDRO         = 10;
} // namespace tex_layer

struct BlockInfo {
    const char* name = "?";
    bool  opaque      = true;   // bloqueia luz e esconde faces dos vizinhos
    bool  transparent = false;  // vai para o mesh transparente
    // Emissao RGB 0..15. Zero em todos = nao emite.
    std::uint8_t emit_r = 0;
    std::uint8_t emit_g = 0;
    std::uint8_t emit_b = 0;
    // Camada da textura por face. Ordem: topo, base, leste(+X), oeste(-X),
    // sul(+Z), norte(-Z).
    std::uint8_t tex_top    = 0;
    std::uint8_t tex_bottom = 0;
    std::uint8_t tex_east   = 0;
    std::uint8_t tex_west   = 0;
    std::uint8_t tex_south  = 0;
    std::uint8_t tex_north  = 0;

    bool emits_light() const { return emit_r || emit_g || emit_b; }
};

// Inicializa a tabela. Chame uma vez antes de qualquer acesso.
void blocks_init();

// Acesso somente leitura.
const BlockInfo& block_info(BlockID id);

inline bool block_is_opaque(BlockID id)      { return block_info(id).opaque; }
inline bool block_is_transparent(BlockID id) { return block_info(id).transparent; }
inline bool block_is_air(BlockID id)         { return id == block_id::AR; }
inline bool block_emits_light(BlockID id)    { return block_info(id).emits_light(); }