#pragma once

#include <string>
#include <glad/glad.h>

// Compila e linka shaders. Retorna 0 em caso de erro (e loga no stderr).
GLuint shader_load(const char* vertex_path, const char* fragment_path);

// Libera um programa de shader.
void shader_destroy(GLuint program);