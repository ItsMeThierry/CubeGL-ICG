#pragma once

#include "chunk.h"
#include "config.h"

#include <unordered_map>
#include <vector>
#include <cstdint>

// World: dono dos chunks carregados. Somente a thread do backend acessa.
// Nesta parte (semana 1) NAO ha geracao nem persistencia: os chunks sao
// criados vazios. A geracao entra na Parte 3; o save/load, na semana 4.

class World {
public:
    explicit World(const Config& cfg);

    // Acesso a blocos em coordenada de mundo.
    // Retorna AR se o chunk nao estiver carregado (borda do mundo carregado).
    BlockID get_block(int wx, int wy, int wz) const;

    // Escreve um bloco. Marca o chunk como dirty e needsMesh.
    // Nao atualiza a luz (isso e da parte de iluminacao).
    void set_block(int wx, int wy, int wz, BlockID id);

    // Acesso direto ao chunk (pode ser nullptr).
    Chunk*       get_chunk(const ChunkPos& p);
    const Chunk* get_chunk(const ChunkPos& p) const;

    // Garante que o chunk exista (cria vazio se preciso). Usado pela geracao.
    Chunk& ensure_chunk(const ChunkPos& p);

    // Atualiza a regiao carregada (esfera 3D) ao redor do jogador.
    // Carrega do mais proximo ao mais distante; descarrega com histerese
    // (raio + 1) para evitar carregar/descarregar em loop na borda.
    // Preenche `out_newly_loaded` e `out_unloaded` com os chunks que
    // entraram e sairam neste tick.
    void update_loaded_region(float px, float py, float pz,
                              std::vector<ChunkPos>& out_newly_loaded,
                              std::vector<ChunkPos>& out_unloaded);

    // Lista de chunks atualmente carregados.
    const std::unordered_map<ChunkPos, Chunk, ChunkPosHash>& chunks() const {
        return chunks_;
    }

    // Versao nao-const, para o backend marcar needsMesh.
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash>& chunks_mutable() {
        return chunks_;
    }

    // Limites do mundo carregado (em blocos de mundo).
    const Config& config() const { return cfg_; }

private:
    bool in_world_bounds(const ChunkPos& p) const;

    Config cfg_;
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks_;
};