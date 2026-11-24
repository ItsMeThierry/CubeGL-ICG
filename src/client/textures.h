#pragma once

#include <glad/glad.h>
#include <string>

// Cria uma GL_TEXTURE_2D_ARRAY a partir de um atlas PNG.
// `tile_size` = tamanho de cada tile (ex.: 16). O atlas deve ter
// largura/altura multiplas de tile_size.
// Retorna 0 em caso de erro.
GLuint texture_array_load_atlas(const std::string& path, int tile_size);