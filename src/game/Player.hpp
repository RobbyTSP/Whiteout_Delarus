#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include "../core/Window.hpp"
#include "../core/Camera.hpp"
#include "../renderer/Renderer.hpp"
#include "TerrainCollider.hpp"
#include "CrevasseBridgeSystem.hpp"
#include <functional>

namespace whiteout::game {

enum class ControlMode {
    FirstPersonMountaineer,
    FreeFlightCamera
};

class WeatherSystem;
class SnowpackSimulation;

class Player {
public:
    Player(core::Camera& camera, const TerrainCollider& collider, const WeatherSystem* weather = nullptr);

    void setWeatherSystem(const WeatherSystem* weather) { m_weather = weather; }

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

    // Step 20: Elasto-Plastic MPM Deformable Snow & Footstep Stamp Tracking
    struct SnowFootstep {
        glm::vec4 posRadius; // xyz = world pos, w = radius / length (m)
        glm::vec4 dirDepth;  // xy = normalized dir (cos/sin yaw), z = indentation depth (m), w = compaction (0..1)
    };
    [[nodiscard]] const std::vector<SnowFootstep>& getRecentFootsteps() const { return m_recentFootsteps; }
    void clearRecentFootsteps() { m_recentFootsteps.clear(); }

    // Step 23: Material-Specific Crampon Footstep Audio Events
    struct AudioFootstepEvent {
        AlpineSurfaceType surfaceType = AlpineSurfaceType::HardFirnSnow;
        float intensity = 1.0f;
        bool isLeftFoot = false;
    };
    [[nodiscard]] const std::vector<AudioFootstepEvent>& getRecentAudioSteps() const { return m_recentAudioSteps; }
    void clearRecentAudioSteps() { m_recentAudioSteps.clear(); }

    // Step 22: Visceral Mountaineer Cryo-Optics & Step 23 Physiology Audio
    [[nodiscard]] bool isGogglesEquipped() const { return m_gogglesEquipped; }
    void toggleGoggles() { m_gogglesEquipped = !m_gogglesEquipped; }
    void setGogglesEquipped(bool equipped) { m_gogglesEquipped = equipped; }

    [[nodiscard]] float getGogglesFog() const { return m_gogglesFog; }
    void setGogglesFog(float fog) { m_gogglesFog = std::clamp(fog, 0.0f, 1.0f); }

    [[nodiscard]] float getGogglesFrost() const { return m_gogglesFrost; }
    void setGogglesFrost(float frost) { m_gogglesFrost = std::clamp(frost, 0.0f, 1.0f); }

    [[nodiscard]] float getSnowBlindness() const { return m_snowBlindness; }
    [[nodiscard]] float getHypoxiaFactor() const { return m_hypoxiaFactor; }
    void setHypoxiaFactor(float h) { m_hypoxiaFactor = std::clamp(h, 0.0f, 1.0f); }

    [[nodiscard]] float getHeartRateBpm() const { return m_heartRateBpm; }
    [[nodiscard]] float getHeartbeatPulse() const { return m_heartbeatPulse; }
    [[nodiscard]] float getExertion() const { return m_exertion; }
    [[nodiscard]] float getBreathPhase() const { return m_breathPhase; }

    [[nodiscard]] renderer::CryoOpticsState getCryoOpticsState() const;

    // Step 24: Stratified Snowpack, Weak-Layer Collapse & Crown Fracture
    [[nodiscard]] const SnowpackSimulation& getSnowpack() const;
    [[nodiscard]] SnowpackSimulation& getSnowpack();
    void triggerSlabFracture(float slabDepth = 0.85f);

    // Step 25: Crevasse Bridges, Bergschrunds & Aluminum Ladders
    [[nodiscard]] const CrevasseBridgeSystem& getBridgeSystem() const { return *m_bridgeSystem; }
    [[nodiscard]] CrevasseBridgeSystem& getBridgeSystem() { return *m_bridgeSystem; }
    [[nodiscard]] bool isCrossingCrevasse() const;
    void setLadderStepCallback(std::function<void(const glm::vec3&, float, int)> cb) { m_onLadderStep = cb; }
    void setBridgeCollapseCallback(std::function<void(const glm::vec3&, float, float)> cb) { m_onBridgeCollapse = cb; }
    void setBridgeCrackCallback(std::function<void(const glm::vec3&, float)> cb) { m_onBridgeCrack = cb; }

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

    // Step 20: Footstep physics & stride state
    float m_lastStepCycle = 0.0f;
    bool m_isLeftFoot = false;
    bool m_wasGrounded = true;
    std::vector<SnowFootstep> m_recentFootsteps;
    std::vector<AudioFootstepEvent> m_recentAudioSteps;

    // Step 22: Visceral Mountaineer Cryo-Optics & Physiology
    bool m_gogglesEquipped = true;
    float m_gogglesFog = 0.0f;       // Condensation mist (0..1)
    float m_gogglesFrost = 0.0f;     // Frozen dendritic ice crystals at edges (0..1)
    float m_snowBlindness = 0.0f;    // Photokeratitis overexposure (0..1)
    float m_hypoxiaFactor = 0.0f;    // Death zone cerebral hypoxia (0..1)
    float m_heartRateBpm = 75.0f;    // Heart rate (60..165 bpm)
    float m_pulsePhase = 0.0f;       // [0, 2pi]
    float m_heartbeatPulse = 0.0f;   // Systole/diastole arterial pulse waveform (0..1)
    float m_breathPhase = 0.0f;      // Respiration cycle [0, 2pi]
    float m_exertion = 0.15f;        // Physical exertion level (0..1)

    // Step 24: Stratified Snowpack Simulation & Weak-Layer Mechanics
    const WeatherSystem* m_weather = nullptr;
    std::unique_ptr<SnowpackSimulation> m_snowpack;

    // Step 25: Crevasse Bridge & Aluminum Ladder System
    std::unique_ptr<CrevasseBridgeSystem> m_bridgeSystem;
    std::function<void(const glm::vec3&, float, int)> m_onLadderStep;
    std::function<void(const glm::vec3&, float, float)> m_onBridgeCollapse;
    std::function<void(const glm::vec3&, float)> m_onBridgeCrack;
};

} // namespace whiteout::game
