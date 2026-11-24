#pragma once

#include "messages.h"
#include "types.h"
#include "config.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>
#include <cstdint>

// ---------------------------------------------------------------------------
// Renderer dos chunks. Um VAO/VBO/EBO por mesh (opaco e transparente) por
// chunk. Faz upload a partir de ChunkMeshUpdate, libera em ChunkUnload e
// desenha com frustum culling.
//
// Roda SOMENTE na thread principal (contexto OpenGL).
// ---------------------------------------------------------------------------
class ChunkRenderer {
public:
    ChunkRenderer();
    ~ChunkRenderer();

    ChunkRenderer(const ChunkRenderer&) = delete;
    ChunkRenderer& operator=(const ChunkRenderer&) = delete;

    // Compila chunk.vert/chunk.frag e carrega o atlas como
    // GL_TEXTURE_2D_ARRAY. `atlas_path` e um PNG com os tiles empilhados
    // verticalmente (16x16 cada) ou uma grade; combinamos no arquivo 8b.
    bool init(const char* chunk_vert_path,
              const char* chunk_frag_path,
              const char* atlas_path,
              const Config& cfg);

    void destroy();

    // Sobe um mesh novo para o chunk (substitui o anterior, se existir).
    // Faz upload dos vetores de Vertex e dos indices.
    void upload_mesh(const ChunkMeshUpdate& up);

    // Libera os buffers de um chunk (ChunkUnload).
    void unload_chunk(const ChunkPos& cp);

    // Desenha todos os chunks, em duas passadas:
    //   1. opacos (depth write on, sem blend);
    //   2. transparentes, ordenados do mais distante ao mais proximo (depth
    //      write off, blend on).
    // Aplica frustum culling por AABB de chunk.
    // `cam_pos` e a posicao do jogador (do snapshot); o deslocamento relativo
    // por chunk e (chunk_origem - cam_pos) para manter precisao numerica.
    void draw(const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& cam_pos,
              float time_of_day,
              const glm::vec3& sky_intensity_color,
              float fog_start, float fog_end,
              const glm::vec3& fog_color,
              const FaceShading& shading);

    // Quantos chunks passaram pelo frustum culling no ultimo frame (opacos e
    // transparentes). Usado no HUD para medir o culling.
    int last_frame_drawn() const             { return last_drawn_opaque_; }
    int last_frame_drawn_transparent() const { return last_drawn_transparent_; }

private:
    struct GpuMesh {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLuint index_count = 0;
        bool   valid = false;
    };
    struct ChunkGpu {
        GpuMesh opaque;
        GpuMesh transparent;
    };

    void destroy_mesh(GpuMesh& m);
    void upload_one(GpuMesh& m,
                    const std::vector<Vertex>& verts,
                    const std::vector<std::uint32_t>& idx);

    GLuint prog_ = 0;
    GLuint tex_array_ = 0;

    std::unordered_map<ChunkPos, ChunkGpu, ChunkPosHash> chunks_;

    // Cache de locais de uniform (evita glGetUniformLocation por frame).
    struct Uniforms {
        GLint model = -1;
        GLint view = -1;
        GLint proj = -1;
        GLint time_of_day = -1;
        GLint sky_color = -1;
        GLint fog_start = -1;
        GLint fog_end = -1;
        GLint fog_color = -1;
        GLint face_shading = -1;   // vec3 no array de 6? ver 8b
        GLint tex_array = -1;
    } uni_;

    struct TranspDraw {
        float dist2;
        ChunkPos pos;
        const GpuMesh* mesh;
    };
    std::vector<TranspDraw> transp_order_;

    int last_drawn_opaque_ = 0;
    int last_drawn_transparent_ = 0;
};