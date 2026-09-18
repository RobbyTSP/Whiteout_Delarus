#include "Player.hpp"
#include "game/SnowpackSimulation.hpp"
#include "game/WeatherSystem.hpp"
#include <iostream>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace whiteout::game {

Player::Player(core::Camera& camera, const TerrainCollider& collider, const WeatherSystem* weather)
    : m_camera(camera), m_collider(collider), m_weather(weather) {
    m_snowpack = std::make_unique<SnowpackSimulation>(m_collider);
    m_bridgeSystem = std::make_unique<CrevasseBridgeSystem>();
    // Start at Everest Base Camp
    teleportToPreset(1);
}

bool Player::isCrossingCrevasse() const {
    return m_bridgeSystem ? m_bridgeSystem->isPlayerOnLadder() : false;
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
        case 2: // Mount Everest Summit Ridge / Hillary Step (8,790m)
            m_position.x = -8500.0f;
            m_position.z = -8002.0f;
            m_yaw = 90.0f;
            m_pitch = 16.0f;
            m_currentLocationName = "Hillary Step (8,790m) [3DGS Fixseile]";
            break;
        case 3: // Lhotse Face / South Col (7,906m)
            m_position.x = -7743.0f;
            m_position.z = -4991.5f;
            m_yaw = -49.4f;
            m_pitch = -18.0f;
            m_currentLocationName = "South Col Camp 4 (7,906m) [3DGS O2-Dump]";
            break;
        case 4: // Ama Dablam Base
            m_position.x = -14966.0f;
            m_position.z = 1606.0f;
            m_yaw = 65.0f;
            m_pitch = 22.0f;
            m_currentLocationName = "Ama Dablam Valley";
            break;
        case 5: // Khumbu Icefall Séracs & Crevasses (5,867m)
            m_position.x = -13800.0f;
            m_position.z = -8600.0f;
            m_yaw = -80.0f;
            m_pitch = -8.0f;
            m_currentLocationName = "Khumbu Icefall Séracs & Crevasses (5,867m)";
            break;
        case 6: // Third Step Climbing Pinnacle & North Ridge (8,690m)
            m_position.x = -8690.0f;
            m_position.z = -7820.0f;
            m_yaw = -65.0f;
            m_pitch = 14.0f;
            m_currentLocationName = "Third Step Climbing Pinnacle (8,690m)";
            break;
        case 7: // Western Cwm "Glutofen" Glacial Amphitheater (Camp 2, 6,400m)
            m_position.x = -11800.0f;
            m_position.z = -7550.0f;
            m_yaw = 15.0f;
            m_pitch = 18.0f;
            m_currentLocationName = "Western Cwm 'Glutofen' (6,400m) - Multi-Bounce GI";
            break;
        case 8: // Mount Everest Summit Plateau (8,848.86m) - 3D Gaussian Splatting
            m_position.x = -8463.5f;
            m_position.z = -8053.5f;
            m_yaw = -77.0f;
            m_pitch = -12.0f;
            m_currentLocationName = "Everest Summit (8,848m) [3DGS Hotspot]";
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
    m_recentFootsteps.clear();
    m_recentAudioSteps.clear();

    if (input.toggleMode) {
        toggleMode();
    }

    if (input.toggleGoggles) {
        toggleGoggles();
        std::cout << "[Player] Glacier Goggles (Cat-4 Polarized) "
                  << (m_gogglesEquipped ? "EQUIPPED [Brewster Glare Shield Active]" : "REMOVED [Extreme Photokeratitis Warning!]")
                  << std::endl;
    }

    if (input.teleportPreset >= 1 && input.teleportPreset <= 8) {
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

    // Step 25: Crevasse Bridge & Sectional Aluminum Ladder Interaction
    float ambientTemp = m_weather ? m_weather->getSummitWeather().temperatureCelsius : -15.0f;
    if (m_bridgeSystem) {
        m_bridgeSystem->update(deltaTime, ambientTemp);
    }
    BridgeInteractionResult bridgeResult{};
    if (m_bridgeSystem) {
        bridgeResult = m_bridgeSystem->resolvePlayer(
            m_position,
            m_velocity,
            input.sprint,
            !m_isGrounded,
            95.0f,
            deltaTime
        );
    }

    // 5. Vertical Physics, Gravity & Jump
    float groundY = m_collider.getHeight(m_position.x, m_position.z);
    if (bridgeResult.isOnStructure) {
        groundY = bridgeResult.supportedHeightY;

        if (bridgeResult.triggeredRungStep && m_onLadderStep) {
            m_onLadderStep(bridgeResult.rungWorldPos, input.sprint ? 1.4f : 1.0f, bridgeResult.rungIndex);
        }
        if (bridgeResult.triggeredCollapse && m_onBridgeCollapse) {
            m_onBridgeCollapse(bridgeResult.collapseWorldPos, bridgeResult.chasmDepth, 1.8f);
        } else if (bridgeResult.triggeredCrack && m_onBridgeCrack) {
            m_onBridgeCrack(bridgeResult.collapseWorldPos, 1.0f);
        }
    }

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

    // Step 20: Elasto-Plastic Footstep Generation
    glm::vec3 fwd = forward;

    // Landing impact marks
    if (!m_wasGrounded && m_isGrounded) {
        float landDepth = input.sprint ? 0.20f : 0.14f;
        SnowFootstep leftLand{}, rightLand{};
        leftLand.posRadius = glm::vec4(m_position - right * 0.18f, 0.32f);
        leftLand.posRadius.y = groundY;
        leftLand.dirDepth = glm::vec4(fwd.x, fwd.z, landDepth, 0.95f);

        rightLand.posRadius = glm::vec4(m_position + right * 0.18f, 0.32f);
        rightLand.posRadius.y = groundY;
        rightLand.dirDepth = glm::vec4(fwd.x, fwd.z, landDepth, 0.95f);

        m_recentFootsteps.push_back(leftLand);
        m_recentFootsteps.push_back(rightLand);

        // Step 23: Landing impact acoustics
        AudioFootstepEvent landAudio{};
        landAudio.surfaceType = m_currentGeology.surfaceType;
        landAudio.intensity = input.sprint ? 1.9f : 1.4f;
        landAudio.isLeftFoot = false;
        m_recentAudioSteps.push_back(landAudio);
    }
    m_wasGrounded = m_isGrounded;

    if (m_isGrounded && speed > 0.3f) {
        m_walkCycle += deltaTime * speed * 2.2f;
        bobY = std::sin(m_walkCycle) * (input.sprint ? 0.06f : 0.035f);
        bobX = std::cos(m_walkCycle * 0.5f) * (input.sprint ? 0.035f : 0.02f);
        m_totalDistance += speed * deltaTime;

        // Every half-stride (approx pi radians ~ 3.14159f) an alternating foot plants
        if (m_walkCycle - m_lastStepCycle >= 3.14159f) {
            m_lastStepCycle = m_walkCycle;
            m_isLeftFoot = !m_isLeftFoot;

            float sideOffset = m_isLeftFoot ? -0.16f : +0.16f;
            glm::vec3 footPos = m_position + right * sideOffset + fwd * 0.12f;
            footPos.y = m_collider.getHeight(footPos.x, footPos.z);

            float stepDepth = input.sprint ? 0.15f : (input.crouch ? 0.04f : 0.09f);
            SnowFootstep step{};
            step.posRadius = glm::vec4(footPos, 0.28f);
            step.dirDepth = glm::vec4(fwd.x, fwd.z, stepDepth, 0.88f);
            m_recentFootsteps.push_back(step);

            // Step 23: Material-specific footstep acoustics (suppressed on ladders in favor of metallic rung pings)
            if (!bridgeResult.isLadder) {
                AudioFootstepEvent stepAudio{};
                stepAudio.surfaceType = m_currentGeology.surfaceType;
                stepAudio.intensity = input.sprint ? 1.4f : (input.crouch ? 0.45f : 1.0f);
                stepAudio.isLeftFoot = m_isLeftFoot;
                m_recentAudioSteps.push_back(stepAudio);
            }
        }
    } else {
        m_walkCycle = 0.0f;
        m_lastStepCycle = 0.0f;
    }

    // 8. Update Camera transform
    glm::vec3 eyePos = m_position + glm::vec3(bobX, m_currentEyeHeight + bobY, 0.0f);
    m_camera.setPosition(eyePos);

    // Calculate camera look direction (with Step 25 ladder balance roll sway)
    float ladderRoll = bridgeResult.lateralSwayRollRad;
    glm::vec3 lookDir;
    lookDir.x = std::cos(glm::radians(m_yaw + ladderRoll * 8.0f)) * std::cos(glm::radians(m_pitch));
    lookDir.y = std::sin(glm::radians(m_pitch));
    lookDir.z = std::sin(glm::radians(m_yaw + ladderRoll * 8.0f)) * std::cos(glm::radians(m_pitch));
    m_camera.setLookAt(eyePos + lookDir);

    // 9. Step 22 (1:1 Part XX): Visceral Mountaineer Cryo-Optics & Alpine Physiology
    float currentSpeed = glm::length(glm::vec2(m_velocity.x, m_velocity.z));
    float speedRatio = std::clamp(currentSpeed / m_sprintSpeed, 0.0f, 1.0f);
    float slopeExertion = (m_currentSlope > 20.0f) ? std::clamp((m_currentSlope - 20.0f) / 35.0f, 0.0f, 1.0f) : 0.0f;
    float altExertion = (m_position.y > 6000.0f) ? std::clamp((m_position.y - 6000.0f) / 2848.0f, 0.0f, 0.6f) : 0.0f;
    float targetExertion = 0.12f + (input.sprint ? 0.45f : 0.20f * speedRatio) + slopeExertion * 0.35f + altExertion;
    m_exertion = glm::mix(m_exertion, std::clamp(targetExertion, 0.1f, 1.0f), std::clamp(3.0f * deltaTime, 0.0f, 1.0f));

    // Respiration & breath condensation
    float breathsPerSec = 0.25f + m_exertion * 0.65f; // ~15 to 54 breaths/min
    m_breathPhase += deltaTime * breathsPerSec * 2.0f * 3.14159265f;
    if (m_breathPhase > 6.2831853f) m_breathPhase -= 6.2831853f;

    float exhalation = std::max(0.0f, std::sin(m_breathPhase));
    float windSpeedKmh = m_currentGeology.jetStreamSpeedKmh;

    if (m_gogglesEquipped) {
        // Warm moist breath condenses on inner goggle lens
        float fogDeposit = exhalation * m_exertion * 0.48f;
        m_gogglesFog += fogDeposit * deltaTime;

        // Ambient ventilation from forward motion and alpine headwind sweeps fog away
        float ventilation = 0.08f + (currentSpeed / 4.5f) * 0.28f + (windSpeedKmh / 120.0f) * 0.20f;
        m_gogglesFog -= ventilation * deltaTime;
        m_gogglesFog = std::clamp(m_gogglesFog, 0.0f, 1.0f);

        // Sub-zero frost crystallization: moisture freezes along cold frame margins
        float frostThreshold = 0.10f;
        if (m_gogglesFog > frostThreshold && m_currentGeology.windChillCelsius < -5.0f) {
            float freezeRate = 0.055f * (m_gogglesFog - frostThreshold) * std::clamp(std::abs(m_currentGeology.windChillCelsius) / 25.0f, 0.2f, 2.5f);
            m_gogglesFrost += freezeRate * deltaTime;
        }

        // Sublimation in dry thin air
        float sublimationRate = 0.006f + (windSpeedKmh / 150.0f) * 0.010f;
        m_gogglesFrost -= sublimationRate * deltaTime;
        m_gogglesFrost = std::clamp(m_gogglesFrost, 0.0f, 1.0f);

        // Recovery from snow blindness when wearing Cat-4 polarized glacier goggles
        m_snowBlindness = glm::mix(m_snowBlindness, 0.0f, std::clamp(3.5f * deltaTime, 0.0f, 1.0f));
    } else {
        // Goggles removed: breath mist clears instantly
        m_gogglesFog = std::max(0.0f, m_gogglesFog - 3.5f * deltaTime);

        // Snow Blindness (Photokeratitis) at altitude (>5,200m) without Cat-4 protection
        float targetBlindness = std::clamp((m_position.y - 5200.0f) / 2600.0f, 0.0f, 1.0f);
        m_snowBlindness = glm::mix(m_snowBlindness, targetBlindness, std::clamp(2.5f * deltaTime, 0.0f, 1.0f));
    }

    // Death Zone Hypoxia (>8,000m)
    float o2 = getOxygenSaturation();
    float baseHypoxia = std::clamp((m_position.y - 7850.0f) / 950.0f, 0.0f, 1.0f);
    float exertionBoost = 1.0f + m_exertion * 0.35f;
    m_hypoxiaFactor = std::clamp(baseHypoxia * exertionBoost, 0.0f, 1.0f);

    // Cardiovascular heart rate & arterial pulse
    float targetBpm = 68.0f + (1.0f - o2 / 100.0f) * 65.0f + m_exertion * 35.0f;
    m_heartRateBpm = glm::mix(m_heartRateBpm, targetBpm, std::clamp(1.5f * deltaTime, 0.0f, 1.0f));

    float bps = m_heartRateBpm / 60.0f;
    m_pulsePhase += deltaTime * bps * 2.0f * 3.14159265f;
    if (m_pulsePhase > 6.2831853f) m_pulsePhase -= 6.2831853f;

    float sinP = std::sin(m_pulsePhase);
    float systole = std::pow(std::max(0.0f, sinP), 5.0f);
    float dicrotic = 0.32f * std::pow(std::max(0.0f, std::sin(m_pulsePhase - 0.75f)), 9.0f);
    m_heartbeatPulse = std::clamp(systole + dicrotic, 0.0f, 1.0f);

    // 10. Step 24 (1:1 Part XXII): Dynamic Snowpack Simulation & Weak-Layer Mechanics
    if (m_snowpack && m_weather) {
        m_snowpack->update(
            deltaTime,
            m_position,
            currentSpeed,
            m_currentSlope,
            m_isGrounded,
            *m_weather
        );
    }
}

void Player::updateFreeFlight(float deltaTime, const core::WindowEventState& input) {
    m_camera.update(deltaTime, input);
    m_position = m_camera.getPosition();
    m_currentGeology = m_collider.getGeologyInfo(m_position.x, m_position.z);
    m_currentSlope = m_currentGeology.slopeDegrees;

    // High altitude hypoxia & pulse dynamics in free flight
    float o2 = getOxygenSaturation();
    float baseHypoxia = std::clamp((m_position.y - 7850.0f) / 950.0f, 0.0f, 1.0f);
    m_hypoxiaFactor = std::clamp(baseHypoxia, 0.0f, 1.0f);

    float targetBpm = 68.0f + (1.0f - o2 / 100.0f) * 55.0f;
    m_heartRateBpm = glm::mix(m_heartRateBpm, targetBpm, std::clamp(1.5f * deltaTime, 0.0f, 1.0f));

    float bps = m_heartRateBpm / 60.0f;
    m_pulsePhase += deltaTime * bps * 2.0f * 3.14159265f;
    if (m_pulsePhase > 6.2831853f) m_pulsePhase -= 6.2831853f;
    float sinP = std::sin(m_pulsePhase);
    m_heartbeatPulse = std::pow(std::max(0.0f, sinP), 5.0f);

    if (m_snowpack && m_weather) {
        m_snowpack->update(
            deltaTime,
            m_position,
            0.0f,
            m_currentSlope,
            false,
            *m_weather
        );
    }
}

const SnowpackSimulation& Player::getSnowpack() const {
    return *m_snowpack;
}

SnowpackSimulation& Player::getSnowpack() {
    return *m_snowpack;
}

void Player::triggerSlabFracture(float slabDepth) {
    if (m_snowpack) {
        m_snowpack->triggerSlabFracture(m_position, slabDepth);
    }
}

renderer::CryoOpticsState Player::getCryoOpticsState() const {
    renderer::CryoOpticsState state{};
    state.gogglesEquipped = m_gogglesEquipped;
    state.gogglesFog = m_gogglesFog;
    state.gogglesFrost = m_gogglesFrost;
    state.snowBlindness = m_snowBlindness;
    state.hypoxiaFactor = m_hypoxiaFactor;
    state.heartbeatPulse = m_heartbeatPulse;
    state.oxygenSaturation = getOxygenSaturation();
    state.altitude = m_position.y;
    return state;
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

        // Step 22: Cryo-Optics HUD telemetry
        ss << " | [G]: Goggles (" << (m_gogglesEquipped ? "Cat-4 Polarized" : "OFF - Photokeratitis Blinding!") << ")";
        if (m_gogglesEquipped) {
            ss << " [Fog: " << static_cast<int>(m_gogglesFog * 100.0f) << "%"
               << ", Frost: " << static_cast<int>(m_gogglesFrost * 100.0f) << "%]";
        }
        ss << " | BPM: " << static_cast<int>(m_heartRateBpm);
        if (isInDeathZone()) {
            ss << " | Hypoxia: " << static_cast<int>(m_hypoxiaFactor * 100.0f) << "%";
        }

        // Step 24: Snowpack Stratification & Avalanche Hazard Telemetry
        if (m_snowpack) {
            ss << " | " << m_snowpack->getSnowpackTelemetry();
        }

        // Step 25: Crevasse Bridge & Aluminum Ladder Telemetry
        if (m_bridgeSystem) {
            std::string bridgeTelem = m_bridgeSystem->getActiveStructureTelemetry();
            if (!bridgeTelem.empty()) {
                ss << " | [" << bridgeTelem << "]";
            }
        }

        // Hotspot Proximity Recognition (Step 18 & Step 21 3DGS)
        float dSummit = glm::length(glm::vec2(m_position.x - (-8462.64f), m_position.z - (-8057.24f)));
        float dHillary = glm::length(glm::vec2(m_position.x - (-8500.0f), m_position.z - (-7995.0f)));
        float dCol = glm::length(glm::vec2(m_position.x - (-7740.0f), m_position.z - (-4995.0f)));
        float dThird = glm::length(glm::vec2(m_position.x - (-8690.0f), m_position.z - (-7820.0f)));
        float dIcefall = glm::length(glm::vec2(m_position.x - (-13800.0f), m_position.z - (-8600.0f)));
        float dCwm = glm::length(glm::vec2(m_position.x - (-11800.0f), m_position.z - (-7550.0f)));

        if (dSummit < 75.0f) {
            ss << " | [HOTSPOT: Mount Everest Summit (8,848m) - 3D Gaussian Splatting Gebetsfahnen & Vermessungsstativ]";
        } else if (dHillary < 75.0f) {
            ss << " | [HOTSPOT: Hillary Step (8,790m) - 3D Gaussian Splatting Fixseil-Tangle & Felssporn]";
        } else if (dCol < 85.0f) {
            ss << " | [HOTSPOT: South Col Camp 4 (7,906m) - 3D Gaussian Splatting O2-Flaschen & Zeltwracks]";
        } else if (dThird < 65.0f) {
            ss << " | [HOTSPOT: Third Step (8,690m) - Vertikaler Kalksteinturm]";
        } else if (dIcefall < 85.0f) {
            ss << " | [HOTSPOT: Khumbu Icefall - Séracs & Spalten]";
        } else if (dCwm < 500.0f) {
            ss << " | [HOTSPOT: Western Cwm (6,400m) - Das Glutofen-Schneelicht GI]";
        } else {
            ss << " | [Ultra 8K PBR | CDLOD: 160m @ 0.25m | 2.91M Tris]";
        }
    } else {
        ss << "[DRONE FLY] "
           << "Alt: " << static_cast<int>(m_position.y) << "m | "
           << "Formation: " << m_currentGeology.formationName << " | "
           << "BPM: " << static_cast<int>(m_heartRateBpm) << " | "
           << "Pos: (" << static_cast<int>(m_position.x) << ", " << static_cast<int>(m_position.z) << ") | "
           << "Far: 150km";
    }

    ss << " | [Tab/V]: Mode | [G]: Goggles | [1-8]: Teleport (1:BC, 2:Hillary, 3:SouthCol, 4:AmaDablam, 5:Icefall, 6:ThirdStep, 7:WesternCwm, 8:Summit)";
    return ss.str();
}

} // namespace whiteout::game
