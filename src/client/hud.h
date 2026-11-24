#pragma once

#include <glad/glad.h>
#include <vector>

// HUD minimo de debug: apenas FPS por enquanto (usa stb_easy_font).
struct Hud {
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint  u_screen = -1;
    bool   visible = true;

    std::vector<float> quad_buf;
    std::vector<float> tri_buf;

    bool init();
    void draw(int width, int height, const char* text);
    void destroy();
};