#include "game/SnowpackSimulation.hpp"
#include "game/TerrainCollider.hpp"
#include "game/WeatherSystem.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

namespace whiteout::game {

SnowpackSimulation::SnowpackSimulation(const TerrainCollider& collider)
    : m_collider(collider) {
}

void SnowpackSimulation::update(
    float deltaTime,
    const glm::vec3& playerPos,
    float playerSpeed,
    float slopeDegrees,
    bool playerGrounded,
    const WeatherSystem& weather
) {
    m_simTime += deltaTime;
    if (m_whumpfCooldown > 0.0f) {
        m_whumpfCooldown = std::max(0.0f, m_whumpfCooldown - deltaTime);
    }

    // 1. Update Metamorphism, Snow Creep & Leeward Wind Drift
    updateStratigraphyAndCreep(deltaTime, slopeDegrees, weather);

    // 2. Compute Stress Balance, Player Surcharge & Stability Index S
    updateStabilityAndSurcharge(playerPos, playerSpeed, slopeDegrees, playerGrounded);

    // 3. Propagate Active Crown Fracture Line (Anrisskante)
    if (m_crownFracture.active) {
        updateCrownFracture(deltaTime);
    }

    m_lastPlayerPos = playerPos;
}

void SnowpackSimulation::updateStratigraphyAndCreep(
    float deltaTime,
    float slopeDegrees,
    const WeatherSystem& weather
) {
    float windSpeed = weather.getWindSpeed(); // km/h
    float blizzard = weather.getBlizzardFactor(); // 0..1
    float slopeRad = glm::radians(std::clamp(slopeDegrees, 0.0f, 85.0f));

    // =========================================================================
    // 1. Leeward Wind Drift & Wind Slab Surcharge (Triebschnee-Akkumulation)
    // =========================================================================
    // Bagnold saltation: snow mass transport scales with windSpeed^3
    float windSpeedMps = windSpeed / 3.6f;
    float transportCapacity = std::pow(std::max(0.0f, windSpeedMps - 4.5f), 2.2f) * 0.008f;
    float driftRateCmH = (transportCapacity + blizzard * 8.5f);
    m_dynamics.windDriftRateCmPerHour = driftRateCmH;

    // Continuous slab thickness variation: wind drift deposits onto leeward slopes
    float driftDeltaM = (driftRateCmH * 0.01f / 3600.0f) * deltaTime;
    m_dynamics.leewardSlabDepthMeters += driftDeltaM;
    m_stratigraphy.slabThicknessMeters = std::clamp(
        0.50f + m_dynamics.leewardSlabDepthMeters * 0.75f,
        0.25f,
        1.60f
    );

    // =========================================================================
    // 2. Visco-Elastic Downhill Snow Creep (Schneekriechen & Gleiten)
    // =========================================================================
    // Empirical alpine creep formula: v_creep = k * sin(slope) * h_slab^1.8
    // Typically 0.5 - 5.0 mm/h on 30° - 45° slopes
    float creepCoeff = 3.2f; // mm/h base
    float creepMmH = creepCoeff * std::sin(slopeRad) * std::pow(m_stratigraphy.slabThicknessMeters, 1.8f);
    m_dynamics.creepVelocityMmPerHour = creepMmH;

    float creepDeltaMeters = (creepMmH * 0.001f / 3600.0f) * deltaTime;
    m_dynamics.cumulativeCreepMeters += creepDeltaMeters;

    // =========================================================================
    // 3. Temperature Gradient Metamorphism (Depth Hoar Weak Layer Faceting)
    // =========================================================================
    // High-altitude cold (-20°C to -40°C) with warmer ground (-2°C) builds large kinetic
    // temperature gradients (> 15 K/m), forming fragile, cup-shaped depth hoar crystals
    // that reduce critical shear strength tau_c
    float baseShearStrength = 1150.0f; // Pa
    float gradientMetamorphism = 1.0f - std::clamp(blizzard * 0.25f + (windSpeed / 180.0f) * 0.20f, 0.0f, 0.45f);
    m_stratigraphy.weakLayerShearStrengthPa = baseShearStrength * gradientMetamorphism;
}

void SnowpackSimulation::updateStabilityAndSurcharge(
    const glm::vec3& playerPos,
    float playerSpeed,
    float slopeDegrees,
    bool playerGrounded
) {
    float slopeRad = glm::radians(std::clamp(slopeDegrees, 0.0f, 85.0f));
    float sinSlope = std::sin(slopeRad);
    float cosSlope = std::cos(slopeRad);
    (void)cosSlope;

    // =========================================================================
    // 1. Natural Gravity-Induced Shear Stress (tau_grav)
    // =========================================================================
    // tau_grav = rho * g * h * sin(slope)
    float g = 9.81f;
    float rho = m_stratigraphy.slabDensityKgM3;
    float h = m_stratigraphy.slabThicknessMeters;
    float tauGrav = rho * g * h * sinSlope; // Pa

    // =========================================================================
    // 2. Mountaineer Dynamic Mechanical Surcharge (Delta tau_player)
    // =========================================================================
    // Mountaineer + expedition pack + oxygen + gear ~ 95 kg
    float playerMassKg = 95.0f;
    float dynamicAmplification = 1.0f;

    if (playerGrounded) {
        if (playerSpeed > 6.0f) {
            dynamicAmplification = 2.4f; // Sprinting / heavy stride impact
        } else if (playerSpeed > 2.0f) {
            dynamicAmplification = 1.5f; // Fast brisk mountaineering walk
        }
    } else {
        dynamicAmplification = 3.6f; // Jump landing dynamic spike
    }

    // Stress bulb dissipation through cohesive slab: tau_player = (M * g * sin(theta)) / (2 * pi * h^2)
    float stressBulbArea = std::max(0.40f, 2.0f * 3.14159f * h * h);
    float deltaTauPlayer = (playerMassKg * g * sinSlope * dynamicAmplification) / stressBulbArea;

    float totalShearStress = tauGrav + deltaTauPlayer;
    float tauC = m_stratigraphy.weakLayerShearStrengthPa;

    // =========================================================================
    // 3. Stability Index S & Avalanche Danger Rating
    // =========================================================================
    // S = tau_c / tau_total
    float sIndex = tauC / std::max(10.0f, totalShearStress);
    m_dynamics.stabilityIndexS = sIndex;

    // European Avalanche Hazard Scale
    if (sIndex > 2.0f) {
        m_dynamics.hazardLevel = AvalancheHazardLevel::Low;
    } else if (sIndex > 1.5f) {
        m_dynamics.hazardLevel = AvalancheHazardLevel::Moderate;
    } else if (sIndex > 1.0f) {
        m_dynamics.hazardLevel = AvalancheHazardLevel::Considerable;
    } else if (sIndex > 0.75f) {
        m_dynamics.hazardLevel = AvalancheHazardLevel::High;
    } else {
        m_dynamics.hazardLevel = AvalancheHazardLevel::VeryHigh;
    }

    // =========================================================================
    // 4. Critical Weak Layer Shear Failure (Spontaneous or Triggered Release)
    // =========================================================================
    // Prime slab avalanche slope angle: 30° to 52°
    bool isAvalancheSlope = (slopeDegrees >= 30.0f && slopeDegrees <= 54.0f);

    if (isAvalancheSlope && sIndex < 1.0f && !m_crownFracture.active && m_whumpfCooldown <= 0.0f) {
        std::cout << "[Snowpack] CRITICAL SHEAR FAILURE! Stability S = " << std::fixed << std::setprecision(2)
                  << sIndex << " on " << static_cast<int>(slopeDegrees) << "° slope! Initiating slab collapse!"
                  << std::endl;
        triggerSlabFracture(playerPos, m_stratigraphy.slabThicknessMeters);
    }
}

void SnowpackSimulation::triggerSlabFracture(const glm::vec3& epicenter, float slabDepth) {
    if (m_crownFracture.active) return;

    m_crownFracture.active = true;
    m_crownFracture.originWorldPos = epicenter;
    m_crownFracture.currentRadiusMeters = 2.0f;
    m_crownFracture.maxRadiusMeters = std::clamp(slabDepth * 220.0f, 80.0f, 280.0f);
    m_crownFracture.propagationSpeedMps = 48.0f; // Rapid anticrack propagation
    m_crownFracture.slabStepHeightMeters = slabDepth;
    m_crownFracture.timeSec = 0.0f;
    m_crownFracture.durationSec = 14.0f;
    m_dynamics.weakLayerCollapsed = true;
    m_whumpfCooldown = 18.0f; // Prevent re-trigger spam

    // Compute slope normal and contour horizontal vector (crack direction)
    glm::vec3 normal = m_collider.getNormal(epicenter.x, epicenter.z);
    m_crownFracture.crackNormal = normal;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 contour = glm::cross(normal, up);
    if (glm::length(contour) > 0.001f) {
        m_crownFracture.propagationDir = glm::normalize(contour);
    } else {
        m_crownFracture.propagationDir = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // Generate jagged Crown Fracture Line path across the face (Anrisskante contour)
    m_crownFracture.fracturePath.clear();
    const int segments = 48;
    float halfWidth = m_crownFracture.maxRadiusMeters * 0.5f;

    for (int i = 0; i <= segments; i++) {
        float t = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f - 1.0f; // [-1, 1]
        float lateral = t * halfWidth;

        // Arching crown curve with high-frequency fractal rock-tear zigzag
        float arch = (1.0f - t * t) * (halfWidth * 0.22f);
        float zigzag = std::sin(t * 28.0f) * 2.4f + std::cos(t * 62.0f) * 1.1f;

        glm::vec3 pt = epicenter
            + m_crownFracture.propagationDir * lateral
            + normal * (arch + zigzag);

        // Snap height to actual DEM terrain surface
        pt.y = m_collider.getHeight(pt.x, pt.z) + 0.15f;
        m_crownFracture.fracturePath.push_back(pt);
    }

    // Notify Audio: Subterranean "Whumpf!" weak-layer collapse sound
    if (m_onWhumpf) {
        m_onWhumpf(epicenter, slabDepth);
    }

    // Notify Audio: Explosive tensile slab fracture snap
    if (m_onCrownSnap) {
        m_onCrownSnap(epicenter, m_crownFracture.maxRadiusMeters);
    }

    // Notify Avalanche Particle System: Release detached slab down mountain couloir
    if (m_onAvalancheRelease) {
        m_onAvalancheRelease(epicenter, m_crownFracture.fracturePath);
    }
}

void SnowpackSimulation::updateCrownFracture(float deltaTime) {
    m_crownFracture.timeSec += deltaTime;

    // Propagate crack outwards
    if (m_crownFracture.currentRadiusMeters < m_crownFracture.maxRadiusMeters) {
        m_crownFracture.currentRadiusMeters += m_crownFracture.propagationSpeedMps * deltaTime;
        if (m_crownFracture.currentRadiusMeters > m_crownFracture.maxRadiusMeters) {
            m_crownFracture.currentRadiusMeters = m_crownFracture.maxRadiusMeters;
            std::cout << "[Snowpack] Crown Fracture fully propagated! Breadth: "
                      << static_cast<int>(m_crownFracture.maxRadiusMeters) << "m across face." << std::endl;
        }
    }

    if (m_crownFracture.timeSec >= m_crownFracture.durationSec) {
        m_crownFracture.active = false;
        m_dynamics.weakLayerCollapsed = false;
    }
}

std::string SnowpackSimulation::getSnowpackTelemetry() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);

    const char* hazardNames[] = {"", "Low (1)", "Moderate (2)", "Considerable (3)", "High (4)", "Very High (5)"};
    int hLevel = static_cast<int>(m_dynamics.hazardLevel);
    hLevel = std::clamp(hLevel, 1, 5);

    ss << "Snow: " << hazardNames[hLevel]
       << " | S=" << m_dynamics.stabilityIndexS
       << " | Slab=" << static_cast<int>(m_stratigraphy.slabThicknessMeters * 100.0f) << "cm"
       << " | Creep=" << std::setprecision(1) << m_dynamics.creepVelocityMmPerHour << "mm/h";

    if (m_crownFracture.active) {
        ss << " | ⚠️ CROWN FRACTURE " << static_cast<int>(m_crownFracture.currentRadiusMeters) << "m ⚠️";
    }

    return ss.str();
}

} // namespace whiteout::game
