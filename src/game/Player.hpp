#pragma once

#include <glm/glm.hpp>
#include <string>
#include "../core/Window.hpp"
#include "../core/Camera.hpp"
#include "TerrainCollider.hpp"

namespace whiteout::game {

enum class ControlMode {
    FirstPersonMountaineer,
    FreeFlightCamera
};

class Player {
public:
    Player(core::Camera& camera, const TerrainCollider& collider);

    void update(float deltaTime, const core::WindowEventState& input);
    void teleportToPreset(int preset);

    [[nodiscard]] ControlMode getMode() const { return m_mode; }
    void toggleMode();

    [[nodiscard]] glm::vec3 getPosition() const { return m_position; }
    [[nodiscard]] float getAltitude() const { return m_position.y; }
    [[nodiscard]] float getCurrentSpeed() const { return glm::length(m_velocity); }
    [[nodiscard]] float getCurrentSlope() const { return m_currentSlope; }
    [[nodiscard]] float getOxygenSaturation() const;
    [[nodiscard]] bool isInDeathZone() const { return m_position.y >= 8000.0f; }
    [[nodiscard]] float getTotalDistance() const { return m_totalDistance; }
    [[nodiscard]] const AlpineGeologyInfo& getGeologyInfo() const { return m_currentGeology; }
    [[nodiscard]] std::string getTelemetryString() const;

private:
    void updateFirstPerson(float deltaTime, const core::WindowEventState& input);
    void updateFreeFlight(float deltaTime, const core::WindowEventState& input);

    core::Camera& m_camera;
    const TerrainCollider& m_collider;
    ControlMode m_mode = ControlMode::FirstPersonMountaineer;

    // First-person state
    glm::vec3 m_position{-15645.0f, 5304.0f, -9751.0f}; // Start at Everest Base Camp!
    glm::vec3 m_velocity{0.0f};
    float m_yaw = 55.0f;
    float m_pitch = 12.0f;

    float m_standingEyeHeight = 1.75f;
    float m_crouchingEyeHeight = 0.95f;
    float m_currentEyeHeight = 1.75f;

    // Physics
    bool m_isGrounded = true;
    float m_verticalVelocity = 0.0f;
    float m_gravity = -19.62f;
    float m_jumpImpulse = 6.5f;

    // Movement speeds (m/s)
    float m_walkSpeed = 4.5f;      // ~16 km/h brisk alpine hike
    float m_sprintSpeed = 10.0f;   // ~36 km/h sprint
    float m_crouchSpeed = 2.0f;
    float m_currentSlope = 0.0f;
    AlpineGeologyInfo m_currentGeology{};

    // Head bobbing
    float m_walkCycle = 0.0f;
    float m_headBobOffset = 0.0f;

    // Stats
    float m_totalDistance = 0.0f;
    std::string m_currentLocationName = "Everest Base Camp";
};

} // namespace whiteout::game
