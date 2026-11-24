// src/backend/mesher.h
#pragma once

#include "chunk.h"
#include "messages.h"
#include "world.h"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Mesher: gera os buffers de vertices/indices de um chunk.
//
// Duas responsabilidades:
//   1. build_chunk_mesh(): monta o mesh opaco e o transparente de um chunk,
//      consultando os vizinhos nas bordas (secao 5.2 do documento).
//   2. compute_affected_chunks(): dado um bloco editado, devolve o conjunto
//      de chunks cujo mesh precisa ser refeito (RF-14). Inclui o proprio
//      chunk e vizinhos quando o bloco esta na borda ou a luz mudou.
//
// Roda SOMENTE na thread do backend. O client so recebe o resultado pronto
// via ChunkMeshUpdate.
// ---------------------------------------------------------------------------

struct MeshData {
    std::vector<Vertex>        opaque_vertices;
    std::vector<std::uint32_t> opaque_indices;
    std::vector<Vertex>        transparent_vertices;
    std::vector<std::uint32_t> transparent_indices;

    void clear() {
        opaque_vertices.clear();
        opaque_indices.clear();
        transparent_vertices.clear();
        transparent_indices.clear();
    }

    bool empty() const {
        return opaque_indices.empty() && transparent_indices.empty();
    }
};

class Mesher {
public:
    Mesher() = default;

    // Monta o mesh do chunk `cp`, que deve estar carregado em `world`.
    // Se o chunk nao existir, devolve um MeshData vazio.
    //
    // Vizinhos ausentes sao tratados como "face invisivel" (o documento diz
    // que a face e omitida nesse caso) e o chunk e marcado needsMesh de novo
    // pelo backend quando o vizinho carregar.
    MeshData build_chunk_mesh(const World& world, const ChunkPos& cp) const;

    // Dado um bloco alterado em (wx, wy, wz), preenche `out` com os chunks
    // cujo mesh precisa ser refeito. Nao marca needsMesh — quem chama decide.
    //
    // Regras:
    //  - o chunk do bloco sempre entra;
    //  - se o bloco esta na borda do chunk (indice local 0 ou 31 em qualquer
    //    eixo), os chunks vizinhos daquela face entram;
    //  - se a luz pode ter mudado (bloco emissor ou bloco opaco removido/
    //    colocado), os vizinhos tambem entram (a luz atravessa a borda).
    void compute_affected_chunks(const World& world,
                                 int wx, int wy, int wz,
                                 std::vector<ChunkPos>& out) const;

private:
    // Adiciona uma face de um bloco ao mesh correto (opaco ou transparente).
    // Recebe a posicao local do bloco, a face e a camada de textura.
    void emit_face(const Chunk& chunk, int lx, int ly, int lz,
                   int face, int layer,
                   bool transparent,
                   MeshData& out) const;

    // Consulta o bloco vizinho a uma face, atravessando chunks via cache dos
    // 6 vizinhos. Devolve AR se o chunk vizinho nao estiver carregado.
    BlockID neighbor_block(const Chunk& self,
                           const Chunk* const neighbors[6],
                           int lx, int ly, int lz,
                           int face) const;
};