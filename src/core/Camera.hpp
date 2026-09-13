#pragma once

#include <glm/glm.hpp>
#include "Window.hpp"

namespace whiteout::core {

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 6000.0f, 12000.0f), float fovDegrees = 65.0f);

    void update(float deltaTime, const WindowEventState& input);
    void setAspectRatio(float aspect) { m_aspectRatio = aspect; }

    [[nodiscard]] glm::mat4 getViewMatrix() const;
    [[nodiscard]] glm::mat4 getProjectionMatrix() const;
    [[nodiscard]] glm::vec3 getPosition() const { return m_position; }
    [[nodiscard]] glm::vec3 getForward() const { return m_forward; }

    void setPosition(const glm::vec3& pos) { m_position = pos; }
    void setLookAt(const glm::vec3& target);

private:
    void updateVectors();

    glm::vec3 m_position;
    glm::vec3 m_forward{0.0f, -0.3f, -0.95f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};

    float m_yaw = -90.0f;
    float m_pitch = -20.0f;
    float m_fov = 65.0f;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearPlane = 0.25f;
    float m_farPlane = 150000.0f; // 150 km visibility for Himalaya vistas

    float m_baseSpeed = 250.0f;     // m/s
    float m_sprintMultiplier = 8.0f; // ~2000 m/s turbo flight
    float m_mouseSensitivity = 0.15f;
};

} // namespace whiteout::core
