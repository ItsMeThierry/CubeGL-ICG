#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

// Skybox procedural (gradiente vertical) em um cubo unitario.
struct Skybox {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint program = 0;
    GLint  u_view_proj = -1;
    GLint  u_zenith = -1;
    GLint  u_horizon = -1;

    bool init();
    void draw(const glm::mat4& view_proj, const glm::vec3& zenith, const glm::vec3& horizon) const;
    void destroy();
};