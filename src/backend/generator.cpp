// src/backend/generator.cpp

// Unico ponto do projeto que define a implementacao do FastNoiseLite.
// Sem isso, o linker reclama de simbolos indefinidos.
#define FNL_IMPL
#include "FastNoiseLite.h"

#include "generator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// Deriva uma seed estavel para um mapa a partir da seed do mundo.
// (FNV-1a de 32 bits aplicado sobre a seed + constante do mapa.)
std::uint32_t derive_seed(std::uint32_t base, std::uint32_t salt) {
    std::uint32_t h = base ^ salt;
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    h ^= h >> 16;
    return h;
}

// FastNoiseLite nao e const nos getters de configuracao, entao guardamos os
// ponteiros como unique_ptr e expomos versoes const via const_cast aqui.
// (A chamada GetNoise() e thread-safe apos configuracao, segundo o README.)
float sample_noise(const FastNoiseLite& n, float x, float y, float z) {
    return const_cast<FastNoiseLite&>(n).GetNoise(x, y, z);
}
float sample_noise_2d(const FastNoiseLite& n, float x, float y) {
    return const_cast<FastNoiseLite&>(n).GetNoise(x, y);
}

// Configura uma instancia com frequencia/oitavas do config.
void configure_noise(FastNoiseLite& n, std::uint32_t seed,
                     float freq, int octaves) {
    n.SetSeed((int)seed);
    n.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    n.SetFrequency(freq);
    n.SetFractalType(FastNoiseLite::FractalType_FBm);
    n.SetFractalOctaves(octaves);
    // Lacunaridade e ganho padrao do FBM do FastNoiseLite ja sao razoaveis.
}

} // namespace

Generator::Generator() = default;
Generator::~Generator() = default;

void Generator::init(const Config& cfg) {
    cfg_ = cfg;
    subsurface_depth_ = 4;

    noise_cont_ = std::make_unique<FastNoiseLite>();
    noise_eros_ = std::make_unique<FastNoiseLite>();
    noise_temp_ = std::make_unique<FastNoiseLite>();
    noise_rel_  = std::make_unique<FastNoiseLite>();

    const auto& R = cfg_.noise;
    configure_noise(*noise_cont_,
                    derive_seed(cfg_.seed, 0x9E3779B9u),
                    R.continental.frequency, R.continental.octaves);
    configure_noise(*noise_eros_,
                    derive_seed(cfg_.seed, 0x85EBCA6Bu),
                    R.erosion.frequency, R.erosion.octaves);
    configure_noise(*noise_temp_,
                    derive_seed(cfg_.seed, 0xC2B2AE35u),
                    R.temperature.frequency, R.temperature.octaves);
    configure_noise(*noise_rel_,
                    derive_seed(cfg_.seed, 0x27D4EB2Fu),
                    R.relief.frequency, R.relief.octaves);
}

// --- Amostragem de ruido (coordenadas de mundo) ---

float Generator::sample_continentalidade(float wx, float wz) const {
    return sample_noise_2d(*noise_cont_, wx, wz);
}
float Generator::sample_erosao(float wx, float wz) const {
    return sample_noise_2d(*noise_eros_, wx, wz);
}
float Generator::sample_temperatura(float wx, float wz) const {
    return sample_noise_2d(*noise_temp_, wx, wz);
}
float Generator::sample_relevo(float wx, float wz) const {
    return sample_noise_2d(*noise_rel_, wx, wz);
}

// --- Curvas ---

float Generator::eval_curve(const std::vector<std::pair<float, float>>& c,
                            float x) {
    if (c.empty()) return 0.0f;
    if (c.size() == 1) return c[0].second;
    if (x <= c.front().first) return c.front().second;
    if (x >= c.back().first)  return c.back().second;

    // Busca linear (curvas sao curtas: 3-5 pontos).
    for (std::size_t i = 0; i + 1 < c.size(); ++i) {
        float x0 = c[i].first,     y0 = c[i].second;
        float x1 = c[i + 1].first, y1 = c[i + 1].second;
        if (x >= x0 && x <= x1) {
            if (x1 == x0) return y0;
            float t = (x - x0) / (x1 - x0);
            return y0 + t * (y1 - y0);
        }
    }
    return c.back().second;
}

// --- Altura e bioma ---

int Generator::compute_height(int wx, int wz) const {
    float cont = sample_continentalidade((float)wx, (float)wz);
    float eros = sample_erosao((float)wx, (float)wz);
    float rel  = sample_relevo((float)wx, (float)wz);

    float base = eval_curve(cfg_.curve_continentalidade, cont);
    float amp  = eval_curve(cfg_.curve_erosao, eros);
    // O relevo varia em [-1, 1]; a amplitude ja e a altura maxima em blocos.
    // Multiplicar direto pode gerar picos negativos; clampamos no fim.
    float h = base + amp * rel;

    int hi = (int)std::lround(h);
    if (hi < 1)   hi = 1;
    if (hi > world_const::WORLD_HEIGHT - 2)
        hi = world_const::WORLD_HEIGHT - 2;
    return hi;
}

BlockID Generator::surface_block(float temperatura) const {
    if (temperatura < -0.25f) return block_id::NEVE;
    if (temperatura >  0.25f) return block_id::AREIA;
    return block_id::GRAMA;
}

BlockID Generator::subsurface_block(float temperatura) const {
    if (temperatura > 0.25f) return block_id::AREIA; // deserto
    return block_id::TERRA;
}

int Generator::surface_height_at(int wx, int wz) const {
    return compute_height(wx, wz);
}

// --- Geracao de chunk ---

void Generator::generate_chunk(const ChunkPos& cp, Chunk& chunk) const {
    // Coordenadas de mundo da base do chunk.
    const int base_x = (int)cp.x * world_const::CHUNK_SIZE;
    const int base_y = (int)cp.y * world_const::CHUNK_SIZE;
    const int base_z = (int)cp.z * world_const::CHUNK_SIZE;

    // Para cada coluna do chunk, calcula altura, bioma e preenche de baixo
    // para cima. Otimizacao simples: as amostras 2D sao por coluna, nao por
    // bloco.
    for (int lz = 0; lz < world_const::CHUNK_SIZE; ++lz) {
        for (int lx = 0; lx < world_const::CHUNK_SIZE; ++lx) {
            const int wx = base_x + lx;
            const int wz = base_z + lz;

            const int h      = compute_height(wx, wz);
            const float temp = sample_temperatura((float)wx, (float)wz);
            const BlockID surf = surface_block(temp);
            const BlockID sub  = subsurface_block(temp);

            for (int ly = 0; ly < world_const::CHUNK_SIZE; ++ly) {
                const int wy = base_y + ly;
                if (wy > h) {
                    // Acima da superficie: agua ou ar.
                    if (wy <= cfg_.sea_level) {
                        chunk.set(lx, ly, lz, block_id::AGUA);
                    } else {
                        chunk.set(lx, ly, lz, block_id::AR);
                    }
                } else if (wy == h) {
                    chunk.set(lx, ly, lz, surf);
                } else if (wy > h - 1 - subsurface_depth_) {
                    // Camada de subsolo.
                    chunk.set(lx, ly, lz, sub);
                } else {
                    // Rocha.
                    chunk.set(lx, ly, lz, block_id::PEDRA);
                }
            }
        }
    }

    // Sem cavernas nesta versao (semana 2, RF-05).
    // Sem minerios, arvores etc.

    chunk.generated = true;
    chunk.needsMesh = true;
    // `dirty` fica como estava: geracao nao e modificacao do jogador.
}