#pragma once

#include <glm/glm.hpp>

// Camera noclip do client. Recebe posicao/orientacao do snapshot do backend,
// mas tambem pode ser movida localmente para testes (sem backend).
struct Camera {
    glm::vec3 position{0.0f, 120.0f, 0.0f};
    float pitch = 0.0f; // graus, -89..89
    float yaw   = -90.0f; // graus (yaw=-90 aponta para -Z)
    float fov   = 70.0f;

    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;

    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
};