#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

glm::vec3 Camera::forward() const {
    float p = glm::radians(pitch);
    float y = glm::radians(yaw);
    return glm::normalize(glm::vec3(
        std::cos(p) * std::cos(y),
        std::sin(p),
        std::cos(p) * std::sin(y)
    ));
}

glm::vec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0, 1, 0)));
}

glm::vec3 Camera::up() const {
    return glm::normalize(glm::cross(right(), forward()));
}

glm::mat4 Camera::view() const {
    return glm::lookAt(position, position + forward(), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::projection(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, 0.05f, 2000.0f);
}