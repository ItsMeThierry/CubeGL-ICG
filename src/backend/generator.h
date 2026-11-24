#pragma once

#include "config.h"
#include "chunk.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <utility>

// Forward declaration para nao vazar o FastNoiseLite no header.
class FastNoiseLite;

// Gerador de mundo (semana 2, versao provisoria 2D).
//
// Cobre apenas o que a "Parte 3" da semana 1 precisa: um terreno simples por
// altura 2D, deterministico para uma seed. Biomas em JSON, cavernas,
// erosao avancada etc. entram na semana 2; este header ja reserva os pontos
// de extensao.
//
// O gerador e SEM ESTADO por chunk: gerar_chunk(cp) so depende de (cp, seed).
// Isso permite regenerar um chunk nao modificado sem salvar em disco.

class Generator {
public:
    Generator();
    ~Generator();

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    // Inicializa com a config. Deriva uma seed por mapa a partir de cfg.seed.
    void init(const Config& cfg);

    // Preenche os blocos do chunk `cp` (que ja deve existir e estar zerado).
    // Nao mexe em luz: a iluminacao e responsabilidade de outra parte.
    void generate_chunk(const ChunkPos& cp, Chunk& chunk) const;

    // Altura da superficie do terreno na coluna (wx, wz), em coordenadas de
    // mundo. Util para posicionar o jogador e (mais tarde) arvores.
    int surface_height_at(int wx, int wz) const;

private:
    // --- ruido 2D ---
    float sample_continentalidade(float wx, float wz) const;
    float sample_erosao(float wx, float wz) const;
    float sample_temperatura(float wx, float wz) const;
    float sample_relevo(float wx, float wz) const;

    // --- curvas ---
    // Interpolacao linear por partes sobre cfg.curves_X (pares x,y ordenados).
    static float eval_curve(const std::vector<std::pair<float, float>>& c,
                            float x);

    // Altura final do terreno na coluna (inteira, ja com clamp).
    int compute_height(int wx, int wz) const;

    // Bloco de superficie escolhido pela temperatura (bioma simplificado).
    BlockID surface_block(float temperatura) const;
    BlockID subsurface_block(float temperatura) const;

    Config cfg_{};

    // FastNoiseLite por mapa. `unique_ptr` para esconder a implementacao.
    std::unique_ptr<FastNoiseLite> noise_cont_;
    std::unique_ptr<FastNoiseLite> noise_eros_;
    std::unique_ptr<FastNoiseLite> noise_temp_;
    std::unique_ptr<FastNoiseLite> noise_rel_;

    // Profundidade do subsolo (fixo nesta versao; biomas JSON na semana 2).
    int subsurface_depth_ = 4;
};