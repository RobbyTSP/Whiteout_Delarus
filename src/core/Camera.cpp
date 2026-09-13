#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace whiteout::core {

Camera::Camera(glm::vec3 position, float fovDegrees)
    : m_position(position), m_fov(fovDegrees) {
    updateVectors();
}

void Camera::updateVectors() {
    glm::vec3 front;
    front.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    front.y = std::sin(glm::radians(m_pitch));
    front.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    m_forward = glm::normalize(front);

    m_right = glm::normalize(glm::cross(m_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_up = glm::normalize(glm::cross(m_right, m_forward));
}

void Camera::setLookAt(const glm::vec3& target) {
    glm::vec3 dir = glm::normalize(target - m_position);
    m_pitch = glm::degrees(std::asin(std::clamp(dir.y, -0.99f, 0.99f)));
    m_yaw = glm::degrees(std::atan2(dir.z, dir.x));
    updateVectors();
}

void Camera::update(float deltaTime, const WindowEventState& input) {
    // Rotation via mouse
    if (input.rightMouseDown) {
        m_yaw += input.mouseDeltaX * m_mouseSensitivity;
        m_pitch -= input.mouseDeltaY * m_mouseSensitivity;
        m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
        updateVectors();
    }

    // Translation
    float speed = m_baseSpeed * (input.sprint ? m_sprintMultiplier : 1.0f) * deltaTime;
    glm::vec3 moveDir(0.0f);

    if (input.moveForward)  moveDir += m_forward;
    if (input.moveBackward) moveDir -= m_forward;
    if (input.moveRight)    moveDir += m_right;
    if (input.moveLeft)     moveDir -= m_right;
    if (input.moveUp)       moveDir += glm::vec3(0.0f, 1.0f, 0.0f);
    if (input.moveDown)     moveDir -= glm::vec3(0.0f, 1.0f, 0.0f);

    if (glm::length(moveDir) > 0.001f) {
        m_position += glm::normalize(moveDir) * speed;
    }
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_forward, m_up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    glm::mat4 proj = glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
    proj[1][1] *= -1.0f; // Vulkan Y-flip
    return proj;
}

} // namespace whiteout::core
