// src/client/renderer.cpp
#include "renderer.h"
#include "shaders.h"
#include "textures.h"
#include "config.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// ===========================================================================
// ChunkRenderer
// ===========================================================================

namespace {

// Extrai os 6 planos de frustum de vp = proj * view (Gribb-Hartmann).
// Planos em ordem: esquerda, direita, baixo, cima, perto, longe.
// Cada plano em vec4 (a,b,c,d), com a*x+b*y+c*z+d >= 0 = "dentro".
struct Frustum {
    glm::vec4 p[6];
};

Frustum extract_frustum(const glm::mat4& vp) {
    Frustum f;
    // Linhas da matriz vp (glm e column-major: vp[c][r] = linha r, coluna c).
    glm::vec4 r0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
    glm::vec4 r1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
    glm::vec4 r2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
    glm::vec4 r3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);
    f.p[0] = r3 + r0; // esquerda
    f.p[1] = r3 - r0; // direita
    f.p[2] = r3 + r1; // baixo
    f.p[3] = r3 - r1; // cima
    f.p[4] = r3 + r2; // perto
    f.p[5] = r3 - r2; // longe
    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(f.p[i]));
        if (len > 0.0f) f.p[i] /= len;
    }
    return f;
}

// Testa um AABB (em coordenadas de mundo) contra o frustum.
bool aabb_in_frustum(const Frustum& f,
                     const glm::vec3& mn, const glm::vec3& mx) {
    for (int i = 0; i < 6; ++i) {
        const glm::vec4& p = f.p[i];
        // Canto "positivo" do AABB em relacao ao plano (o mais distante).
        glm::vec3 pos(
            p.x >= 0.0f ? mx.x : mn.x,
            p.y >= 0.0f ? mx.y : mn.y,
            p.z >= 0.0f ? mx.z : mn.z);
        if (p.x * pos.x + p.y * pos.y + p.z * pos.z + p.w < 0.0f)
            return false;
    }
    return true;
}

} // namespace

ChunkRenderer::ChunkRenderer() = default;
ChunkRenderer::~ChunkRenderer() { destroy(); }

bool ChunkRenderer::init(const char* chunk_vert_path,
                         const char* chunk_frag_path,
                         const char* atlas_path,
                         const Config& cfg) {
    (void)cfg;
    prog_ = shader_load(chunk_vert_path, chunk_frag_path);
    if (!prog_) return false;

    tex_array_ = texture_array_load_atlas(atlas_path, 16);
    if (!tex_array_) {
        std::fprintf(stderr, "[chunk_renderer] falha ao carregar atlas '%s'\n",
                     atlas_path);
        shader_destroy(prog_);
        prog_ = 0;
        return false;
    }

    // Cache de uniforms.
    uni_.model         = glGetUniformLocation(prog_, "u_model");
    uni_.view          = glGetUniformLocation(prog_, "u_view");
    uni_.proj          = glGetUniformLocation(prog_, "u_proj");
    uni_.time_of_day   = glGetUniformLocation(prog_, "u_time_of_day");
    uni_.sky_color     = glGetUniformLocation(prog_, "u_sky_color");
    uni_.fog_start     = glGetUniformLocation(prog_, "u_fog_start");
    uni_.fog_end       = glGetUniformLocation(prog_, "u_fog_end");
    uni_.fog_color     = glGetUniformLocation(prog_, "u_fog_color");
    uni_.face_shading  = glGetUniformLocation(prog_, "u_face_shading");
    uni_.tex_array     = glGetUniformLocation(prog_, "u_atlas");

    // Liga a textura no slot 0 de uma vez (o shader usa uniform sampler).
    glUseProgram(prog_);
    glUniform1i(uni_.tex_array, 0);
    glUseProgram(0);

    return true;
}

void ChunkRenderer::destroy() {
    for (auto& kv : chunks_) {
        destroy_mesh(kv.second.opaque);
        destroy_mesh(kv.second.transparent);
    }
    chunks_.clear();
    if (tex_array_) { glDeleteTextures(1, &tex_array_); tex_array_ = 0; }
    if (prog_)      { shader_destroy(prog_); prog_ = 0; }
}

void ChunkRenderer::destroy_mesh(GpuMesh& m) {
    if (m.ebo) { glDeleteBuffers(1, &m.ebo); m.ebo = 0; }
    if (m.vbo) { glDeleteBuffers(1, &m.vbo); m.vbo = 0; }
    if (m.vao) { glDeleteVertexArrays(1, &m.vao); m.vao = 0; }
    m.index_count = 0;
    m.valid = false;
}

void ChunkRenderer::upload_one(GpuMesh& m,
                               const std::vector<Vertex>& verts,
                               const std::vector<std::uint32_t>& idx) {
    // Se nao ha geometria, libera o mesh antigo.
    if (verts.empty() || idx.empty()) {
        destroy_mesh(m);
        return;
    }

    if (!m.vao) glGenVertexArrays(1, &m.vao);
    if (!m.vbo) glGenBuffers(1, &m.vbo);
    if (!m.ebo) glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);

    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(Vertex)),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(idx.size() * sizeof(std::uint32_t)),
                 idx.data(), GL_STATIC_DRAW);

    const GLsizei stride = (GLsizei)sizeof(Vertex);
    // pos (u8vec3) como inteiro
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 3, GL_UNSIGNED_BYTE, stride,
                           (void*)offsetof(Vertex, pos_x));
    // face_and_corner (u8) como inteiro
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_BYTE, stride,
                           (void*)offsetof(Vertex, face_uv));
    // layer (u8) como inteiro
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, stride,
                           (void*)offsetof(Vertex, layer));
    // light (u16) como inteiro
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_SHORT, stride,
                           (void*)offsetof(Vertex, light));

    glBindVertexArray(0);

    m.index_count = (GLuint)idx.size();
    m.valid = true;
}

void ChunkRenderer::upload_mesh(const ChunkMeshUpdate& up) {
    ChunkGpu& g = chunks_[up.pos];
    upload_one(g.opaque, up.opaque_vertices, up.opaque_indices);
    upload_one(g.transparent, up.transparent_vertices,
               up.transparent_indices);
}

void ChunkRenderer::unload_chunk(const ChunkPos& cp) {
    auto it = chunks_.find(cp);
    if (it == chunks_.end()) return;
    destroy_mesh(it->second.opaque);
    destroy_mesh(it->second.transparent);
    chunks_.erase(it);
}

void ChunkRenderer::draw(const glm::mat4& view, const glm::mat4& proj,
                         const glm::vec3& cam_pos,
                         float time_of_day,
                         const glm::vec3& sky_intensity_color,
                         float fog_start, float fog_end,
                         const glm::vec3& fog_color,
                         const FaceShading& shading) {
    if (!prog_) return;

    // Frustum em coordenadas de mundo, para o culling.
    glm::mat4 vp = proj * view;
    Frustum frustum = extract_frustum(vp);

    // View rotacional (sem translacao) para desenhar com deslocamento
    // relativo a camera. Ver explicacao no topo da mensagem.
    glm::mat4 view_rot = glm::mat4(glm::mat3(view));

    // Uniforms comuns.
    glUseProgram(prog_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_array_);

    glUniformMatrix4fv(uni_.view, 1, GL_FALSE, glm::value_ptr(view_rot));
    glUniformMatrix4fv(uni_.proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1f(uni_.time_of_day, time_of_day);
    glUniform3fv(uni_.sky_color, 1, glm::value_ptr(sky_intensity_color));
    glUniform1f(uni_.fog_start, fog_start);
    glUniform1f(uni_.fog_end, fog_end);
    glUniform3fv(uni_.fog_color, 1, glm::value_ptr(fog_color));

    // Tabela de sombreamento por face, na ordem do shader (face_index).
    float fs[6] = {
        shading.topo, shading.base, shading.leste,
        shading.oeste, shading.sul, shading.norte
    };
    glUniform1fv(uni_.face_shading, 6, fs);

    const float S = (float)world_const::CHUNK_SIZE;

    // -------- Passada 1: opacos --------
    last_drawn_opaque_ = 0;
    last_drawn_transparent_ = 0;

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);


    for (auto& kv : chunks_) {
        const ChunkPos& cp = kv.first;
        ChunkGpu& g = kv.second;
        if (!g.opaque.valid) continue;

        glm::vec3 origin((float)cp.x * S, (float)cp.y * S, (float)cp.z * S);
        glm::vec3 mn = origin;
        glm::vec3 mx = origin + glm::vec3(S);
        if (!aabb_in_frustum(frustum, mn, mx)) continue;

        ++last_drawn_opaque_;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), origin - cam_pos);
        glUniformMatrix4fv(uni_.model, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(g.opaque.vao);
        glDrawElements(GL_TRIANGLES, (GLsizei)g.opaque.index_count,
                       GL_UNSIGNED_INT, nullptr);
    }

    // -------- Passada 2: transparentes, ordenados do mais distante --------
    transp_order_.clear();
    for (const auto& kv : chunks_) {
        const ChunkPos& cp = kv.first;
        if (!kv.second.transparent.valid) continue;

        glm::vec3 origin((float)cp.x * S, (float)cp.y * S, (float)cp.z * S);
        glm::vec3 mn = origin;
        glm::vec3 mx = origin + glm::vec3(S);
        if (!aabb_in_frustum(frustum, mn, mx)) continue;

        glm::vec3 center = origin + glm::vec3(S * 0.5f);
        glm::vec3 d = center - cam_pos;
        float dist2 = glm::dot(d, d);
        transp_order_.push_back(TranspDraw{ dist2, cp, &kv.second.transparent });
    }

    last_drawn_transparent_ = (int)transp_order_.size();

    std::sort(transp_order_.begin(), transp_order_.end(),
              [](const TranspDraw& a, const TranspDraw& b) {
                  return a.dist2 > b.dist2; // mais distante primeiro
              });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE); // faces transparentes podem ser vistas por tras

    for (const auto& e : transp_order_) {
        glm::vec3 origin((float)e.pos.x * S, (float)e.pos.y * S, (float)e.pos.z * S);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), origin - cam_pos);
        glUniformMatrix4fv(uni_.model, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(e.mesh->vao);
        glDrawElements(GL_TRIANGLES, (GLsizei)e.mesh->index_count,
                       GL_UNSIGNED_INT, nullptr);
    }

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
}