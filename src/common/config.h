#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Configuracao carregada de config.json.
//
// Campos das proximas partes (biomas em JSON, cavernas, ciclo dia/noite mais
// completo) serao adicionados conforme forem necessarios. Chaves desconhecidas
// no JSON sao ignoradas; chaves com tipo errado geram aviso e usam o default.
// ---------------------------------------------------------------------------

// Parametros de um mapa de ruido 2D/3D.
struct NoiseMapConfig {
    float frequency = 0.001f;
    int   octaves   = 4;
    // Usados apenas por cavernas (semana 2), por ora nao lidos pelo gerador.
    float threshold = 0.0f;
    float epsilon   = 0.0f;
};

struct NoiseConfig {
    NoiseMapConfig continental;
    NoiseMapConfig erosion;
    NoiseMapConfig temperature;
    NoiseMapConfig relief;
    NoiseMapConfig caves_large;
    NoiseMapConfig tunnels;
};

// Fatores de sombreamento por face (secao 5.3). A ordem dos campos segue a
// convencao de face_index (topo, base, leste, oeste, sul, norte) apenas para
// documentacao; no shader o mapeamento e feito por indice de face.
struct FaceShading {
    float topo  = 1.0f;
    float base  = 0.5f;
    float leste = 0.8f;
    float oeste = 0.6f;
    float sul   = 0.9f;
    float norte = 0.7f;
};

// Par (x, y) de uma curva linear por partes.
using CurvePoint = std::pair<float, float>;

struct Config {
    // janela
    int   win_w = 1280;
    int   win_h = 720;
    bool  vsync = true;
    bool  fullscreen = true; 
    float fov   = 70.0f;

    // jogador
    float player_speed         = 12.0f;
    float player_shift_mult    = 3.0f;
    float player_sensitivity   = 0.1f;
    float player_raycast_range = 8.0f;

    // mundo
    std::uint32_t seed       = 12345;
    std::string   world_name = "mundo1";
    int  chunk_radius        = 5;
    int  sea_level           = 96;
    int  cave_surface_margin = 6;

    // ruido
    NoiseConfig noise;

    // curvas (pares x,y ordenados por x)
    std::vector<CurvePoint> curve_continentalidade;
    std::vector<CurvePoint> curve_erosao;

    // ciclo dia/noite
    float day_duration_s = 240.0f;
    float day_initial_t  = 0.35f;

    // fog
    float fog_start_relative = 0.6f;

    // sombreamento por face (usado no shader)
    FaceShading face_shading;
};

// Carrega o config.json do diretorio de trabalho. Se falhar, mantem os
// defaults e imprime uma mensagem clara em stderr (RNF-08).
// Retorna true se carregou com sucesso.
bool config_load(const char* path, Config& out);