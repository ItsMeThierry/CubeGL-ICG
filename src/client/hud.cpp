#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

#include "hud.h"
#include "shaders.h"

#include <algorithm>
#include <cstdio>

static constexpr int HUD_MAX_QUADS = 4096;

bool Hud::init() {
    program = shader_load("assets/shaders/hud.vert", "assets/shaders/hud.frag");
    if (!program) return false;

    u_screen = glGetUniformLocation(program, "u_screen");
    quad_buf.resize((size_t)HUD_MAX_QUADS * 16, 0.0f);
    tri_buf.reserve((size_t)HUD_MAX_QUADS * 24);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(6 * HUD_MAX_QUADS * 4 * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    return true;
}

void Hud::draw(int width, int height, const char* text) {
    if (!visible || !program) return;

    char tmp[512];
    std::snprintf(tmp, sizeof(tmp), "%s", text);

    unsigned char color[4] = {255, 255, 255, 255};
    int quads = stb_easy_font_print(0.0f, 0.0f, tmp, color, quad_buf.data(), (int)(quad_buf.size() * sizeof(float)));
    quads = std::min(quads, HUD_MAX_QUADS);

    glUseProgram(program);
    glUniform2f(u_screen, (float)width, (float)height);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Converte cada quad (4 vertices) em dois triangulos (6 vertices).
    tri_buf.clear();
    for (int q = 0; q < quads; ++q) {
        const float* v = &quad_buf[(size_t)q * 4 * 4];
        const float* v0 = v + 0;
        const float* v1 = v + 4;
        const float* v2 = v + 8;
        const float* v3 = v + 12;
        tri_buf.insert(tri_buf.end(), v0, v0 + 4);
        tri_buf.insert(tri_buf.end(), v1, v1 + 4);
        tri_buf.insert(tri_buf.end(), v2, v2 + 4);
        tri_buf.insert(tri_buf.end(), v0, v0 + 4);
        tri_buf.insert(tri_buf.end(), v2, v2 + 4);
        tri_buf.insert(tri_buf.end(), v3, v3 + 4);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(tri_buf.size() * sizeof(float)), tri_buf.data());
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(tri_buf.size() / 4));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void Hud::destroy() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (program) shader_destroy(program);
    vbo = vao = program = 0;
    u_screen = -1;
}