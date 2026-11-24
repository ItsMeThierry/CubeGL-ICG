#include "textures.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstdio>
#include <vector>

GLuint texture_array_load_atlas(const std::string& path, int tile_size) {
    int w = 0, h = 0, ch = 0;
    // Flip vertical: o OpenGL le a textura com v=0 embaixo, mas o PNG tem
    // a linha 0 no topo. Sem flip, as faces laterais do bloco (que mapeiam
    // y=0 -> v=0, y=1 -> v=1) mostram o tile de cabeca para baixo.
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) {
        std::fprintf(stderr, "[texture] falha ao carregar '%s'\n", path.c_str());
        return 0;
    }
    if (tile_size <= 0 || w % tile_size != 0 || h % tile_size != 0) {
        std::fprintf(stderr, "[texture] atlas '%s' nao e multiplo de %d\n", path.c_str(), tile_size);
        stbi_image_free(data);
        return 0;
    }

    int tiles_x = w / tile_size;
    int tiles_y = h / tile_size;
    int layer_count = tiles_x * tiles_y;

    std::vector<unsigned char> layer(tile_size * tile_size * 4);
    std::vector<unsigned char> all;
    all.reserve((size_t)layer_count * layer.size());

    // Empacota cada tile como uma camada, na ordem linha a linha.
    for (int ty = 0; ty < tiles_y; ++ty) {
        for (int tx = 0; tx < tiles_x; ++tx) {
            for (int y = 0; y < tile_size; ++y) {
                for (int x = 0; x < tile_size; ++x) {
                    int sx = tx * tile_size + x;
                    int sy = ty * tile_size + y;
                    size_t src = ((size_t)sy * w + sx) * 4;
                    size_t dst = ((size_t)y * tile_size + x) * 4;
                    for (int c = 0; c < 4; ++c) layer[dst + c] = data[src + c];
                }
            }
            all.insert(all.end(), layer.begin(), layer.end());
        }
    }
    stbi_image_free(data);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
                 tile_size, tile_size, layer_count,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, all.data());

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return tex;
}