#include "skybox.h"
#include "shaders.h"

#include <glm/gtc/type_ptr.hpp>

bool Skybox::init() {
    program = shader_load("assets/shaders/skybox.vert", "assets/shaders/skybox.frag");
    if (!program) return false;

    u_view_proj = glGetUniformLocation(program, "u_view_proj");
    u_zenith    = glGetUniformLocation(program, "u_zenith");
    u_horizon   = glGetUniformLocation(program, "u_horizon");

    // Cubo unitario centralizado na origem (apenas posicoes).
    const float v[] = {
        -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1,
        -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
        -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1,
        -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1,
        -1,-1, 1, -1, 1, 1, -1, 1,-1, -1, 1,-1, -1,-1,-1, -1,-1, 1,
         1,-1, 1,  1, 1, 1,  1, 1,-1,  1, 1,-1,  1,-1,-1,  1,-1, 1,
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    return true;
}

void Skybox::draw(const glm::mat4& view_proj, const glm::vec3& zenith, const glm::vec3& horizon) const {
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glUseProgram(program);

    glUniformMatrix4fv(u_view_proj, 1, GL_FALSE, glm::value_ptr(view_proj));
    glUniform3fv(u_zenith, 1, glm::value_ptr(zenith));
    glUniform3fv(u_horizon, 1, glm::value_ptr(horizon));

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void Skybox::destroy() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (program) shader_destroy(program);
    vbo = vao = program = 0;
    u_view_proj = u_zenith = u_horizon = -1;
}