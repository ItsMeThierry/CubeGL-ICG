#include "config.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>

namespace {

using json = nlohmann::json;

// --- leitores tolerantes: se a chave faltar ou tiver tipo errado, mantem ---

void warn(const char* sec, const char* key, const char* expected) {
    std::fprintf(stderr,
        "[config] chave '%s.%s' deveria ser %s; usando default.\n",
        sec, key, expected);
}

bool get_section(const json& j, const char* sec, const json*& out) {
    if (!j.contains(sec) || !j[sec].is_object()) return false;
    out = &j[sec];
    return true;
}

void read_int(const json& j, const char* sec, const char* key, int& dst) {
    const json* s = nullptr;
    if (!get_section(j, sec, s)) return;
    if (!s->contains(key)) return;
    if (!(*s)[key].is_number_integer()) { warn(sec, key, "inteiro"); return; }
    dst = (*s)[key].get<int>();
}

void read_float(const json& j, const char* sec, const char* key, float& dst) {
    const json* s = nullptr;
    if (!get_section(j, sec, s)) return;
    if (!s->contains(key)) return;
    if (!(*s)[key].is_number()) { warn(sec, key, "numero"); return; }
    dst = (*s)[key].get<float>();
}

void read_bool(const json& j, const char* sec, const char* key, bool& dst) {
    const json* s = nullptr;
    if (!get_section(j, sec, s)) return;
    if (!s->contains(key)) return;
    if (!(*s)[key].is_boolean()) { warn(sec, key, "booleano"); return; }
    dst = (*s)[key].get<bool>();
}

void read_str(const json& j, const char* sec, const char* key, std::string& dst) {
    const json* s = nullptr;
    if (!get_section(j, sec, s)) return;
    if (!s->contains(key)) return;
    if (!(*s)[key].is_string()) { warn(sec, key, "string"); return; }
    dst = (*s)[key].get<std::string>();
}

void read_u32(const json& j, const char* sec, const char* key,
              std::uint32_t& dst) {
    const json* s = nullptr;
    if (!get_section(j, sec, s)) return;
    if (!s->contains(key)) return;
    if (!(*s)[key].is_number_integer() && !(*s)[key].is_number_unsigned()) {
        warn(sec, key, "inteiro nao-negativo");
        return;
    }
    dst = (std::uint32_t)(*s)[key].get<long long>();
}

// --- ruido ---

void read_noise_map(const json& j, const char* name, NoiseMapConfig& dst) {
    const json* s = nullptr;
    if (!get_section(j, "ruido", s)) return;
    if (!s->contains(name) || !(*s)[name].is_object()) return;

    const json& m = (*s)[name];
    if (m.contains("frequencia") && m["frequencia"].is_number())
        dst.frequency = m["frequencia"].get<float>();
    else if (m.contains("frequencia"))
        std::fprintf(stderr, "[config] ruido.%s.frequencia deveria ser numero\n", name);

    if (m.contains("oitavas") && m["oitavas"].is_number_integer())
        dst.octaves = m["oitavas"].get<int>();
    else if (m.contains("oitavas"))
        std::fprintf(stderr, "[config] ruido.%s.oitavas deveria ser inteiro\n", name);

    if (m.contains("limiar") && m["limiar"].is_number())
        dst.threshold = m["limiar"].get<float>();
    if (m.contains("epsilon") && m["epsilon"].is_number())
        dst.epsilon = m["epsilon"].get<float>();
}

void read_noise(const json& j, NoiseConfig& n) {
    read_noise_map(j, "continentalidade", n.continental);
    read_noise_map(j, "erosao",           n.erosion);
    read_noise_map(j, "temperatura",      n.temperature);
    read_noise_map(j, "relevo",           n.relief);
    read_noise_map(j, "cavernas_grandes", n.caves_large);
    read_noise_map(j, "tuneis",           n.tunnels);
}

// --- curvas ---

void read_curve(const json& j, const char* name,
                std::vector<CurvePoint>& dst) {
    const json* s = nullptr;
    if (!get_section(j, "curvas", s)) return;
    if (!s->contains(name) || !(*s)[name].is_array()) return;

    std::vector<CurvePoint> pts;
    for (const auto& item : (*s)[name]) {
        if (!item.is_array() || item.size() != 2) continue;
        if (!item[0].is_number() || !item[1].is_number()) continue;
        pts.emplace_back(item[0].get<float>(), item[1].get<float>());
    }
    // Ordena por x, para o eval_curve assumir o invariante.
    std::sort(pts.begin(), pts.end(),
              [](const CurvePoint& a, const CurvePoint& b) {
                  return a.first < b.first;
              });
    if (!pts.empty()) dst = std::move(pts);
}

// --- sombreamento por face ---

void read_shading(const json& j, FaceShading& fs) {
    const json* s = nullptr;
    if (!get_section(j, "sombreamento_faces", s)) return;
    auto rd = [&](const char* k, float& dst) {
        if (s->contains(k) && (*s)[k].is_number())
            dst = (*s)[k].get<float>();
    };
    rd("topo",  fs.topo);
    rd("base",  fs.base);
    rd("leste", fs.leste);
    rd("oeste", fs.oeste);
    rd("sul",   fs.sul);
    rd("norte", fs.norte);
}

} // namespace

bool config_load(const char* path, Config& out) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr,
            "[config] nao foi possivel abrir '%s'. Usando defaults.\n", path);
        return false;
    }

    json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        std::fprintf(stderr,
            "[config] JSON invalido em '%s': %s. Usando defaults.\n",
            path, e.what());
        return false;
    }

    // janela
    read_int  (j, "janela", "largura", out.win_w);
    read_int  (j, "janela", "altura",  out.win_h);
    read_bool (j, "janela", "vsync",   out.vsync);
    read_bool (j, "janela", "fullscreen", out.fullscreen);   // <-- novo
    read_float(j, "janela", "fov",     out.fov);

    // jogador
    read_float(j, "jogador", "velocidade",          out.player_speed);
    read_float(j, "jogador", "multiplicador_shift", out.player_shift_mult);
    read_float(j, "jogador", "sensibilidade",       out.player_sensitivity);
    read_float(j, "jogador", "alcance_raycast",     out.player_raycast_range);

    // mundo
    read_u32(j, "mundo", "seed",         out.seed);
    read_str(j, "mundo", "nome",         out.world_name);
    read_int(j, "mundo", "raio_chunks",  out.chunk_radius);
    read_int(j, "mundo", "nivel_do_mar", out.sea_level);
    read_int(j, "mundo", "margem_superficie_cavernas", out.cave_surface_margin);

    // ruido
    read_noise(j, out.noise);

    // curvas
    read_curve(j, "continentalidade", out.curve_continentalidade);
    read_curve(j, "erosao",           out.curve_erosao);

    // ciclo dia/noite
    read_float(j, "ciclo_dia", "duracao_segundos", out.day_duration_s);
    read_float(j, "ciclo_dia", "hora_inicial",     out.day_initial_t);

    // fog
    read_float(j, "fog", "inicio_relativo", out.fog_start_relative);

    // sombreamento por face
    read_shading(j, out.face_shading);

    return true;
}