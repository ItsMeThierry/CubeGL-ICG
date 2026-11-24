#include "blocks.h"

#include <array>

namespace {

std::array<BlockInfo, block_id::COUNT> g_blocks;
bool g_init = false;

// Atalho para criar entradas com menos ruido visual.
BlockInfo make(const char* name, bool opaque, bool transparent,
               int r, int g, int b,
               int top, int bottom, int east, int west, int south, int north) {
    BlockInfo i;
    i.name        = name;
    i.opaque      = opaque;
    i.transparent = transparent;
    i.emit_r = (std::uint8_t)r;
    i.emit_g = (std::uint8_t)g;
    i.emit_b = (std::uint8_t)b;
    i.tex_top    = (std::uint8_t)top;
    i.tex_bottom = (std::uint8_t)bottom;
    i.tex_east   = (std::uint8_t)east;
    i.tex_west   = (std::uint8_t)west;
    i.tex_south  = (std::uint8_t)south;
    i.tex_north  = (std::uint8_t)north;
    return i;
}

void populate() {
    using namespace tex_layer;

    // Ar: transparente, nao opaco, sem textura util (camada 0 so para evitar zero).
    g_blocks[block_id::AR] = make("ar", /*op*/false, /*tr*/true, 0,0,0,
                                  0,0,0,0,0,0);

    // Pedra, terra: todas as faces iguais.
    g_blocks[block_id::PEDRA] = make("pedra", true, false, 0,0,0,
                                     PEDRA,PEDRA,PEDRA,PEDRA,PEDRA,PEDRA);
    g_blocks[block_id::TERRA] = make("terra", true, false, 0,0,0,
                                     TERRA,TERRA,TERRA,TERRA,TERRA,TERRA);

    // Grama: topo diferente, base = terra, lados = grama_lado.
    g_blocks[block_id::GRAMA] = make("grama", true, false, 0,0,0,
                                     GRAMA_TOPO, TERRA,
                                     GRAMA_LADO, GRAMA_LADO, GRAMA_LADO, GRAMA_LADO);

    g_blocks[block_id::AREIA] = make("areia", true, false, 0,0,0,
                                     AREIA,AREIA,AREIA,AREIA,AREIA,AREIA);
    g_blocks[block_id::NEVE]  = make("neve", true, false, 0,0,0,
                                     NEVE,NEVE,NEVE,NEVE,NEVE,NEVE);

    // Agua: nao opaca, transparente. Deixa a luz passar (a regra de luz
    // usa opaque == false para permitir passagem).
    g_blocks[block_id::AGUA] = make("agua", /*op*/false, /*tr*/true, 0,0,0,
                                    AGUA,AGUA,AGUA,AGUA,AGUA,AGUA);

    // Lanternas: opacas (bloqueiam luz) e emitem na sua cor.
    // Azul usa (3,6,15) como sugerido no documento, para nao parecer escuro.
    g_blocks[block_id::LANTERNA_VERM] =
        make("lanterna_vermelha", true, false, 15,0,0,
             LANTERNA_VERM,LANTERNA_VERM,LANTERNA_VERM,LANTERNA_VERM,LANTERNA_VERM,LANTERNA_VERM);
    g_blocks[block_id::LANTERNA_AZUL] =
        make("lanterna_azul", true, false, 3,6,15,
             LANTERNA_AZUL,LANTERNA_AZUL,LANTERNA_AZUL,LANTERNA_AZUL,LANTERNA_AZUL,LANTERNA_AZUL);
    g_blocks[block_id::LANTERNA_VERD] =
        make("lanterna_verde", true, false, 0,15,0,
             LANTERNA_VERD,LANTERNA_VERD,LANTERNA_VERD,LANTERNA_VERD,LANTERNA_VERD,LANTERNA_VERD);

    // Vidro: nao opaco, transparente. Deixa a luz passar.
    g_blocks[block_id::VIDRO] = make("vidro", /*op*/false, /*tr*/true, 0,0,0,
                                     VIDRO,VIDRO,VIDRO,VIDRO,VIDRO,VIDRO);
}

} // namespace

void blocks_init() {
    if (g_init) return;
    populate();
    g_init = true;
}

const BlockInfo& block_info(BlockID id) {
    if (id >= block_id::COUNT) id = block_id::AR; // defensivo
    return g_blocks[id];
}