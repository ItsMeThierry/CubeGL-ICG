#include "player.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace {
// Mesma convencao do Camera::forward() do client:
//   forward = (cos p cos y, sin p, cos p sin y)
// yaw = 0 -> +X; yaw = -90 -> -Z.
glm::vec3 forward_from_angles(float yaw_deg, float pitch_deg) {
    float p = glm::radians(pitch_deg);
    float y = glm::radians(yaw_deg);
    return glm::normalize(glm::vec3(
        std::cos(p) * std::cos(y),
        std::sin(p),
        std::cos(p) * std::sin(y)
    ));
}
} // namespace

void Player::update(const InputState& in, float dt) {
    if (dt <= 0.0f) return;

    // --- Orientacao (mesma convencao do Camera::look do client) ---
    yaw   += in.mouse_dx * sensitivity;
    pitch -= in.mouse_dy * sensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    if (yaw >  180.0f) yaw -= 360.0f;
    if (yaw < -180.0f) yaw += 360.0f;

    // --- Movimento ---
    float spd = speed * (in.shift_held ? shift_mult : 1.0f);

    // Frente/tras no plano horizontal (como o Camera::move_local faz com flat_fwd).
    glm::vec3 fwd = forward_from_angles(yaw, 0.0f);
    fwd.y = 0.0f;
    if (glm::length(fwd) > 1e-6f) fwd = glm::normalize(fwd);
    else fwd = glm::vec3(0.0f, 0.0f, -1.0f);

    // Direita no plano horizontal: mesma formula do Camera::right() achatado.
    glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));

    glm::vec3 up(0.0f, 1.0f, 0.0f);

    // Eixos locais: x = direita, y = cima (mundo), z = frente.
    glm::vec3 vel = fwd   * in.move_forward
                  + right * in.move_right
                  + up    * in.move_up;

    if (glm::length(vel) > 1e-6f) vel = glm::normalize(vel) * spd;

    px += vel.x * dt;
    py += vel.y * dt;
    pz += vel.z * dt;
}

void Player::fill_snapshot(PlayerSnapshot& s) const {
    s.px = px;
    s.py = py;
    s.pz = pz;
    s.yaw   = yaw;
    s.pitch = pitch;
}