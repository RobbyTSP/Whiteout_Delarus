#include "Player.hpp"
#include <iostream>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace whiteout::game {

Player::Player(core::Camera& camera, const TerrainCollider& collider)
    : m_camera(camera), m_collider(collider) {
    // Start at Everest Base Camp
    teleportToPreset(1);
}

void Player::teleportToPreset(int preset) {
    switch (preset) {
        case 1: // Everest Base Camp South (Khumbu Glacier)
            m_position.x = -15645.0f;
            m_position.z = -9751.0f;
            m_yaw = 50.0f;
            m_pitch = 18.0f;
            m_currentLocationName = "Everest Base Camp (South)";
            break;
        case 2: // Mount Everest Summit Ridge / Hillary Step
            m_position.x = -8560.0f;
            m_position.z = -7945.0f;
            m_yaw = 35.0f;
            m_pitch = -4.0f;
            m_currentLocationName = "Mount Everest Summit Ridge (8,848m)";
            break;
        case 3: // Lhotse Face / Camp 3
            m_position.x = -7740.0f;
            m_position.z = -4995.0f;
            m_yaw = -45.0f;
            m_pitch = 12.0f;
            m_currentLocationName = "Lhotse Face / South Col";
            break;
        case 4: // Ama Dablam Base
            m_position.x = -14966.0f;
            m_position.z = 1606.0f;
            m_yaw = 65.0f;
            m_pitch = 22.0f;
            m_currentLocationName = "Ama Dablam Valley";
            break;
        default:
            return;
    }

    float groundY = m_collider.getHeight(m_position.x, m_position.z);
    m_position.y = groundY;
    m_verticalVelocity = 0.0f;
    m_velocity = glm::vec3(0.0f);
    m_isGrounded = true;

    m_currentGeology = m_collider.getGeologyInfo(m_position.x, m_position.z);
    m_currentSlope = m_currentGeology.slopeDegrees;

    glm::vec3 eyePos = m_position + glm::vec3(0.0f, m_currentEyeHeight, 0.0f);
    m_camera.setPosition(eyePos);

    glm::vec3 lookDir;
    lookDir.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    lookDir.y = std::sin(glm::radians(m_pitch));
    lookDir.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    m_camera.setLookAt(eyePos + lookDir);

    std::cout << "[Player] Teleported to: " << m_currentLocationName
              << " | Altitude: " << static_cast<int>(groundY) << " m"
              << " | " << m_currentGeology.formationName << std::endl;
}

void Player::toggleMode() {
    if (m_mode == ControlMode::FirstPersonMountaineer) {
        m_mode = ControlMode::FreeFlightCamera;
        std::cout << "[Player] Switched to: Free-Flight Drone Camera Mode [Fly freely across Himalaya]" << std::endl;
    } else {
        m_mode = ControlMode::FirstPersonMountaineer;
        // Drop down to terrain surface at current X, Z
        glm::vec3 camPos = m_camera.getPosition();
        m_position.x = camPos.x;
        m_position.z = camPos.z;
        m_position.y = m_collider.getHeight(camPos.x, camPos.z);
        m_verticalVelocity = 0.0f;
        m_velocity = glm::vec3(0.0f);
        m_isGrounded = true;
        std::cout << "[Player] Switched to: First-Person Mountaineer Mode [Walking on 1:1 Terrain]" << std::endl;
    }
}

float Player::getOxygenSaturation() const {
    // Barometric formula approximation for effective oxygen percentage
    // Sea level = 100%, Base Camp (5364m) ~ 52%, Everest Summit (8848m) ~ 31%
    float alt = std::max(0.0f, m_position.y);
    float saturation = 100.0f * std::exp(-alt / 7600.0f);
    return std::clamp(saturation, 10.0f, 100.0f);
}

void Player::update(float deltaTime, const core::WindowEventState& input) {
    if (input.toggleMode) {
        toggleMode();
    }

    if (input.teleportPreset >= 1 && input.teleportPreset <= 4) {
        teleportToPreset(input.teleportPreset);
    }

    if (m_mode == ControlMode::FirstPersonMountaineer) {
        updateFirstPerson(deltaTime, input);
    } else {
        updateFreeFlight(deltaTime, input);
    }
}

void Player::updateFirstPerson(float deltaTime, const core::WindowEventState& input) {
    // 1. Mouse look (Pitch & Yaw)
    float mouseSens = 0.14f;
    m_yaw += input.mouseDeltaX * mouseSens;
    m_pitch -= input.mouseDeltaY * mouseSens;
    m_pitch = std::clamp(m_pitch, -88.0f, 88.0f);

    // Calculate horizontal forward and right directions
    float yawRad = glm::radians(m_yaw);
    glm::vec3 forward(std::cos(yawRad), 0.0f, std::sin(yawRad));
    forward = glm::normalize(forward);
    glm::vec3 right(-forward.z, 0.0f, forward.x);

    // 2. Input movement direction
    glm::vec3 moveDir(0.0f);
    if (input.moveForward)  moveDir += forward;
    if (input.moveBackward) moveDir -= forward;
    if (input.moveRight)    moveDir += right;
    if (input.moveLeft)     moveDir -= right;

    if (glm::length(moveDir) > 0.001f) {
        moveDir = glm::normalize(moveDir);
    }

    // 3. Terrain Slope & Geomorphological Surface Calculation
    m_currentGeology = m_collider.getGeologyInfo(m_position.x, m_position.z);
    m_currentSlope = m_currentGeology.slopeDegrees;

    // Alpine surface footstep physics:
    float surfaceFriction = 1.0f;
    glm::vec3 surfaceSlide(0.0f);

    if (m_currentGeology.surfaceType == AlpineSurfaceType::TalusScreeSlope) {
        // Unstable scree slope: shifting gravel causes downhill sliding and traction reduction
        surfaceFriction = 0.78f;
        glm::vec3 normal = m_collider.getNormal(m_position.x, m_position.z);
        surfaceSlide = glm::vec3(normal.x, 0.0f, normal.z) * 1.6f;
    } else if (m_currentGeology.surfaceType == AlpineSurfaceType::GlacialBlueIce) {
        // Glacial blue ice: reduced turning traction
        surfaceFriction = 0.88f;
    }

    // Realistic alpine slope penalty: steep climbs slow down movement
    float slopeSpeedModifier = 1.0f;
    if (m_currentSlope > 20.0f) {
        // Slow down proportionally on steep slopes
        slopeSpeedModifier = std::max(0.18f, 1.0f - ((m_currentSlope - 20.0f) / 45.0f));
    }

    // High-altitude jet stream headwind drag
    glm::vec3 jetStreamDir(-0.92f, 0.0f, -0.38f);
    float windHeadwind = glm::dot(moveDir, -jetStreamDir);
    float windPenalty = 1.0f;
    if (m_position.y > 6000.0f && windHeadwind > 0.0f) {
        windPenalty = 1.0f - (m_currentGeology.jetStreamSpeedKmh / 260.0f) * windHeadwind;
        windPenalty = std::max(0.35f, windPenalty);
    }

    // Target walking/sprinting speed
    float targetSpeed = (input.sprint ? m_sprintSpeed : (input.crouch ? m_crouchSpeed : m_walkSpeed))
                        * slopeSpeedModifier * surfaceFriction * windPenalty;
    glm::vec3 targetVelocity = moveDir * targetSpeed + surfaceSlide;

    // Smooth horizontal acceleration/braking
    float accel = m_isGrounded ? 12.0f : 2.5f; // reduced air control
    m_velocity.x = glm::mix(m_velocity.x, targetVelocity.x, std::clamp(accel * deltaTime, 0.0f, 1.0f));
    m_velocity.z = glm::mix(m_velocity.z, targetVelocity.z, std::clamp(accel * deltaTime, 0.0f, 1.0f));

    // 4. Update horizontal position
    glm::vec3 nextPos = m_position + m_velocity * deltaTime;
    m_position.x = nextPos.x;
    m_position.z = nextPos.z;

    // 5. Vertical Physics, Gravity & Jump
    float groundY = m_collider.getHeight(m_position.x, m_position.z);

    if (m_isGrounded) {
        if (input.jump) {
            m_verticalVelocity = m_jumpImpulse;
            m_isGrounded = false;
        } else {
            // Smoothly snap to terrain contour
            float heightDiff = groundY - m_position.y;
            if (std::abs(heightDiff) < 2.5f) {
                m_position.y = groundY;
            } else {
                // Falling off a ledge / serac
                m_isGrounded = false;
            }
            m_verticalVelocity = 0.0f;
        }
    }

    if (!m_isGrounded) {
        m_verticalVelocity += m_gravity * deltaTime;
        m_position.y += m_verticalVelocity * deltaTime;

        // Ground collision check
        if (m_position.y <= groundY) {
            m_position.y = groundY;
            m_verticalVelocity = 0.0f;
            m_isGrounded = true;
        }
    }

    // 6. Crouching eye height transition
    float targetEyeHeight = input.crouch ? m_crouchingEyeHeight : m_standingEyeHeight;
    m_currentEyeHeight = glm::mix(m_currentEyeHeight, targetEyeHeight, std::clamp(10.0f * deltaTime, 0.0f, 1.0f));

    // 7. Head Bobbing for realistic mountaineering immersion
    float speed = glm::length(glm::vec2(m_velocity.x, m_velocity.z));
    float bobX = 0.0f;
    float bobY = 0.0f;

    if (m_isGrounded && speed > 0.3f) {
        m_walkCycle += deltaTime * speed * 2.2f;
        bobY = std::sin(m_walkCycle) * (input.sprint ? 0.06f : 0.035f);
        bobX = std::cos(m_walkCycle * 0.5f) * (input.sprint ? 0.035f : 0.02f);
        m_totalDistance += speed * deltaTime;
    } else {
        m_walkCycle = 0.0f;
    }

    // 8. Update Camera transform
    glm::vec3 eyePos = m_position + glm::vec3(bobX, m_currentEyeHeight + bobY, 0.0f);
    m_camera.setPosition(eyePos);

    // Calculate camera look direction
    glm::vec3 lookDir;
    lookDir.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    lookDir.y = std::sin(glm::radians(m_pitch));
    lookDir.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    m_camera.setLookAt(eyePos + lookDir);
}

void Player::updateFreeFlight(float deltaTime, const core::WindowEventState& input) {
    m_camera.update(deltaTime, input);
    m_position = m_camera.getPosition();
    m_currentGeology = m_collider.getGeologyInfo(m_position.x, m_position.z);
    m_currentSlope = m_currentGeology.slopeDegrees;
}

std::string Player::getTelemetryString() const {
    std::stringstream ss;
    float speedKmh = getCurrentSpeed() * 3.6f;
    float o2 = getOxygenSaturation();

    if (m_mode == ControlMode::FirstPersonMountaineer) {
        ss << "[1ST PERSON] " << m_currentLocationName << " | "
           << "Alt: " << static_cast<int>(m_position.y) << "m | "
           << "Spd: " << std::fixed << std::setprecision(1) << speedKmh << " km/h | "
           << "Slope: " << static_cast<int>(m_currentSlope) << "° | "
           << "O2: " << static_cast<int>(o2) << "%";
        if (isInDeathZone()) {
            ss << " [DEATH ZONE > 8000m!]";
        }
        ss << " | Formation: " << m_currentGeology.formationName
           << " | Footing: " << m_currentGeology.surfaceTypeName
           << " | Jet Stream: WNW " << static_cast<int>(m_currentGeology.jetStreamSpeedKmh) << " km/h"
           << " (Chill: " << static_cast<int>(m_currentGeology.windChillCelsius) << "°C)";
    } else {
        ss << "[DRONE FLY] "
           << "Alt: " << static_cast<int>(m_position.y) << "m | "
           << "Formation: " << m_currentGeology.formationName << " | "
           << "Pos: (" << static_cast<int>(m_position.x) << ", " << static_cast<int>(m_position.z) << ") | "
           << "Far: 150km";
    }

    ss << " | [Tab/V]: Switch Mode | [1-4]: Teleport";
    return ss.str();
}

} // namespace whiteout::game
