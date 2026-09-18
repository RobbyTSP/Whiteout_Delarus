#include "game/CrevasseBridgeSystem.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace whiteout::game {

float SnowBridge::getSpanLength() const {
    return glm::length(anchorB - anchorA);
}

glm::vec3 SnowBridge::getCenter() const {
    return (anchorA + anchorB) * 0.5f;
}

glm::vec3 SnowBridge::getSpanDirection() const {
    glm::vec3 diff = anchorB - anchorA;
    float len = glm::length(diff);
    return (len > 0.001f) ? (diff / len) : glm::vec3(0.0f, 0.0f, 1.0f);
}

float AluminumLadder::getSpanLength() const {
    return glm::length(anchorB - anchorA);
}

glm::vec3 AluminumLadder::getCenter() const {
    return (anchorA + anchorB) * 0.5f;
}

glm::vec3 AluminumLadder::getSpanDirection() const {
    glm::vec3 diff = anchorB - anchorA;
    float len = glm::length(diff);
    return (len > 0.001f) ? (diff / len) : glm::vec3(0.0f, 0.0f, 1.0f);
}

CrevasseBridgeSystem::CrevasseBridgeSystem() {
    initDefaultExpeditionStructures();
}

void CrevasseBridgeSystem::initDefaultExpeditionStructures() {
    m_ladders.clear();
    m_snowBridges.clear();
    m_bergschrunds.clear();

    // =========================================================================
    // Structure 1: Khumbu Icefall Main Expedition Ladder (Preset 5 Hotspot)
    // Spanning the 25m abyssal crevasse directly in front of the mountaineer route
    // =========================================================================
    AluminumLadder ladderKhumbuMain{};
    ladderKhumbuMain.id = "ladder_khumbu_main";
    ladderKhumbuMain.locationName = "Khumbu Icefall Lower Crevasse Crossing (5,867m)";
    ladderKhumbuMain.anchorA = glm::vec3(-13800.0f, 5867.2f, -8602.0f);
    ladderKhumbuMain.anchorB = glm::vec3(-13799.0f, 5866.5f, -8611.5f);
    ladderKhumbuMain.width = 0.48f;
    ladderKhumbuMain.numSections = 4;
    ladderKhumbuMain.numRungs = 31;
    ladderKhumbuMain.elasticModulusPa = 69e9f; // 6061-T6 aluminum alloy
    ladderKhumbuMain.momentOfInertiaM4 = 3.6e-7f;
    ladderKhumbuMain.naturalFrequencyHz = 2.85f;
    m_ladders.push_back(ladderKhumbuMain);

    // =========================================================================
    // Structure 2: Fragile Firn Snow Bridge (Adjacent to Main Ladder)
    // A natural sintered firn arch spanning the same 25m slot 8 meters east
    // =========================================================================
    SnowBridge bridgeKhumbuFragile{};
    bridgeKhumbuFragile.id = "bridge_khumbu_fragile";
    bridgeKhumbuFragile.locationName = "Khumbu Icefall Natural Firn Arch (5,867m)";
    bridgeKhumbuFragile.anchorA = glm::vec3(-13792.0f, 5867.1f, -8602.0f);
    bridgeKhumbuFragile.anchorB = glm::vec3(-13791.5f, 5866.4f, -8611.5f);
    bridgeKhumbuFragile.width = 2.2f;
    bridgeKhumbuFragile.thickness = 0.58f;
    bridgeKhumbuFragile.densityKgM3 = 390.0f;
    bridgeKhumbuFragile.criticalStrengthPa = 32000.0f; // 32 kPa ultimate tensile/bending stress
    bridgeKhumbuFragile.currentSagMeters = 0.09f;
    bridgeKhumbuFragile.chasmDepthMeters = 25.0f;
    bridgeKhumbuFragile.status = BridgeStatus::Intact;
    m_snowBridges.push_back(bridgeKhumbuFragile);

    // =========================================================================
    // Structure 3: Khumbu Mid-Icefall Angled Crevasse Ladder
    // Higher up the route crossing active shearing crevasses
    // =========================================================================
    AluminumLadder ladderKhumbuMid{};
    ladderKhumbuMid.id = "ladder_khumbu_mid";
    ladderKhumbuMid.locationName = "Khumbu Icefall Mid-Sérac Traverse (5,886m)";
    ladderKhumbuMid.anchorA = glm::vec3(-13785.0f, 5886.0f, -8570.0f);
    ladderKhumbuMid.anchorB = glm::vec3(-13783.0f, 5885.5f, -8578.0f);
    ladderKhumbuMid.width = 0.48f;
    ladderKhumbuMid.numSections = 3;
    ladderKhumbuMid.numRungs = 26;
    ladderKhumbuMid.naturalFrequencyHz = 3.10f;
    m_ladders.push_back(ladderKhumbuMid);

    // =========================================================================
    // Structure 4: Khumbu Vertical Sérac Wall Ladder
    // Ascending an overhanging 5m glacial ice wall
    // =========================================================================
    AluminumLadder ladderKhumbuSerac{};
    ladderKhumbuSerac.id = "ladder_khumbu_serac";
    ladderKhumbuSerac.locationName = "Khumbu Sérac Wall Vertical Ascender (5,910m)";
    ladderKhumbuSerac.anchorA = glm::vec3(-13755.0f, 5908.0f, -8525.0f);
    ladderKhumbuSerac.anchorB = glm::vec3(-13753.5f, 5913.8f, -8520.5f);
    ladderKhumbuSerac.width = 0.48f;
    ladderKhumbuSerac.numSections = 2;
    ladderKhumbuSerac.numRungs = 20;
    ladderKhumbuSerac.naturalFrequencyHz = 3.65f;
    m_ladders.push_back(ladderKhumbuSerac);

    // =========================================================================
    // Structure 5: Lhotse Face Bergschrund Ladder (Camp 3 Approach, 8,160m)
    // Huge tectonic rim fissure separating stationary wall ice from moving glacier
    // =========================================================================
    AluminumLadder ladderLhotse{};
    ladderLhotse.id = "ladder_lhotse_bergschrund";
    ladderLhotse.locationName = "Lhotse Face Great Bergschrund Ladder (8,160m)";
    ladderLhotse.anchorA = glm::vec3(-8005.0f, 8159.0f, -5206.0f);
    ladderLhotse.anchorB = glm::vec3(-8000.0f, 8163.5f, -5198.0f);
    ladderLhotse.width = 0.52f;
    ladderLhotse.numSections = 4;
    ladderLhotse.numRungs = 34;
    ladderLhotse.naturalFrequencyHz = 2.60f;
    m_ladders.push_back(ladderLhotse);

    // Structure 6: Lhotse Cornice Snow Bridge
    SnowBridge bridgeLhotse{};
    bridgeLhotse.id = "bridge_lhotse_cornice";
    bridgeLhotse.locationName = "Lhotse Bergschrund Wind-Cornice Arch (8,162m)";
    bridgeLhotse.anchorA = glm::vec3(-7990.0f, 8161.0f, -5204.0f);
    bridgeLhotse.anchorB = glm::vec3(-7988.0f, 8164.2f, -5196.0f);
    bridgeLhotse.width = 1.8f;
    bridgeLhotse.thickness = 0.72f;
    bridgeLhotse.densityKgM3 = 420.0f;
    bridgeLhotse.criticalStrengthPa = 42000.0f;
    bridgeLhotse.currentSagMeters = 0.06f;
    bridgeLhotse.chasmDepthMeters = 34.0f;
    bridgeLhotse.status = BridgeStatus::Intact;
    m_snowBridges.push_back(bridgeLhotse);

    // Bergschrund Fissures
    BergschrundFissure bsKhumbu{};
    bsKhumbu.id = "bergschrund_khumbu_rim";
    bsKhumbu.locationName = "Khumbu Upper Icefall Extensional Rim";
    bsKhumbu.rimUpper = glm::vec3(-12500.0f, 6330.0f, -7800.0f);
    bsKhumbu.rimLower = glm::vec3(-12505.0f, 6325.0f, -7806.0f);
    bsKhumbu.gapWidthMeters = 6.2f;
    bsKhumbu.verticalDropMeters = 5.0f;
    bsKhumbu.depthMeters = 28.0f;
    bsKhumbu.openingVelocityMmPerDay = 2.4f;
    m_bergschrunds.push_back(bsKhumbu);

    BergschrundFissure bsLhotse{};
    bsLhotse.id = "bergschrund_lhotse_face";
    bsLhotse.locationName = "Lhotse Face Main Bergschrund (Camp 3 Transition)";
    bsLhotse.rimUpper = glm::vec3(-8000.0f, 8164.0f, -5198.0f);
    bsLhotse.rimLower = glm::vec3(-8005.0f, 8159.0f, -5206.0f);
    bsLhotse.gapWidthMeters = 7.5f;
    bsLhotse.verticalDropMeters = 5.2f;
    bsLhotse.depthMeters = 35.0f;
    bsLhotse.openingVelocityMmPerDay = 1.6f;
    m_bergschrunds.push_back(bsLhotse);

    std::cout << "[CrevasseBridgeSystem] Initialized " << m_ladders.size() << " aluminum expedition ladders, "
              << m_snowBridges.size() << " firn snow bridges, and "
              << m_bergschrunds.size() << " tectonic bergschrund fissures." << std::endl;
}

void CrevasseBridgeSystem::update(float deltaTime, float ambientTempC) {
    m_simTime += deltaTime;

    // =========================================================================
    // 1. Visco-Elastic Snow Bridge Creep Sag
    // =========================================================================
    // Visco-elastic creep rate increases with warmer temperatures (solar radiation)
    float tempKelvin = std::max(230.0f, ambientTempC + 273.15f);
    float creepFactor = 1.0e-5f * std::exp((tempKelvin - 263.15f) * 0.08f);

    for (auto& bridge : m_snowBridges) {
        if (bridge.status == BridgeStatus::Intact || bridge.status == BridgeStatus::Creaking) {
            float span = bridge.getSpanLength();
            float selfWeightW = bridge.densityKgM3 * 9.81f * bridge.width * bridge.thickness;
            float sagRate = creepFactor * (selfWeightW * span * span) / std::max(10.0f, bridge.thickness);
            bridge.currentSagMeters += sagRate * deltaTime;

            // Maximum allowable sagging before catastrophic ductile shear
            if (bridge.currentSagMeters > bridge.thickness * 0.45f && bridge.status == BridgeStatus::Intact) {
                bridge.status = BridgeStatus::Creaking;
            }
        } else if (bridge.status == BridgeStatus::Collapsing) {
            bridge.collapseTimer += deltaTime;
            if (bridge.collapseTimer > 1.8f) {
                bridge.status = BridgeStatus::Destroyed;
            }
        }
    }

    // =========================================================================
    // 2. Aluminum Ladder Damped Sway Decay
    // =========================================================================
    for (auto& ladder : m_ladders) {
        if (!ladder.playerOnLadder) {
            // Under-damped harmonic decay when vacant
            float damping = 3.2f; // zeta ~ 0.15
            ladder.swayVelocity -= (ladder.swayAngleRadians * 40.0f + ladder.swayVelocity * damping) * deltaTime;
            ladder.swayAngleRadians += ladder.swayVelocity * deltaTime;
            ladder.currentDeflectionMeters = glm::mix(ladder.currentDeflectionMeters, 0.0f, std::clamp(6.0f * deltaTime, 0.0f, 1.0f));
            ladder.verticalOscillationM = glm::mix(ladder.verticalOscillationM, 0.0f, std::clamp(5.0f * deltaTime, 0.0f, 1.0f));
        }
    }
}

BridgeInteractionResult CrevasseBridgeSystem::resolvePlayer(
    const glm::vec3& playerPos,
    const glm::vec3& playerVelocity,
    bool isSprinting,
    bool isJumpingOrLanding,
    float mountaineerMassKg,
    float deltaTime
) {
    BridgeInteractionResult result{};
    m_activeBridgeIndex = -1;
    m_activeLadderIndex = -1;

    float playerHorizSpeed = glm::length(glm::vec2(playerVelocity.x, playerVelocity.z));
    float dynamicMultiplier = 1.0f;
    if (isJumpingOrLanding) {
        dynamicMultiplier = 3.2f; // Hard jump landing impact
    } else if (isSprinting) {
        dynamicMultiplier = 2.1f; // Heavy sprinting stride impact
    } else if (playerHorizSpeed > 1.5f) {
        dynamicMultiplier = 1.4f; // Brisk mountaineering pace
    }

    float g = 9.81f;
    float effectiveLoadForceN = mountaineerMassKg * g * dynamicMultiplier;

    // =========================================================================
    // 1. Resolve Aluminum Ladders
    // =========================================================================
    for (size_t i = 0; i < m_ladders.size(); i++) {
        auto& ladder = m_ladders[i];
        ladder.playerOnLadder = false;

        glm::vec3 spanVec = ladder.anchorB - ladder.anchorA;
        float spanLen = glm::length(spanVec);
        if (spanLen < 0.5f) continue;
        glm::vec3 u = spanVec / spanLen;

        // Player relative to start
        glm::vec3 toPlayer = playerPos - ladder.anchorA;
        float progress = glm::dot(toPlayer, u) / spanLen;

        // Check if player is along the longitudinal span
        if (progress >= -0.05f && progress <= 1.05f) {
            glm::vec3 centerlinePt = ladder.anchorA + u * (progress * spanLen);
            glm::vec3 lateralOffset = playerPos - centerlinePt;
            float distLateral = glm::length(lateralOffset);

            // Within ladder width boundary (+/- margin)
            if (distLateral <= ladder.width * 0.95f) {
                ladder.playerOnLadder = true;
                ladder.playerProgress = std::clamp(progress, 0.0f, 1.0f);
                m_activeLadderIndex = static_cast<int>(i);

                // Euler-Bernoulli static midspan deflection: delta = F * L^3 / (48 * E * I)
                float E = ladder.elasticModulusPa;
                float I = ladder.momentOfInertiaM4;
                float midspanDelta = (effectiveLoadForceN * spanLen * spanLen * spanLen) / (48.0f * E * I);

                // Deflection at current position x: delta(x) = 4 * delta_mid * s * (1 - s)
                float s = ladder.playerProgress;
                float staticDeflection = 4.0f * midspanDelta * s * (1.0f - s);
                ladder.currentDeflectionMeters = glm::mix(ladder.currentDeflectionMeters, staticDeflection, std::clamp(12.0f * deltaTime, 0.0f, 1.0f));

                // Dynamic vertical oscillation
                float oscFreq = ladder.naturalFrequencyHz;
                float oscAmp = 0.018f * (playerHorizSpeed / 2.0f) * std::sin(s * 3.14159f);
                ladder.verticalOscillationM = oscAmp * std::sin(m_simTime * 2.0f * 3.14159f * oscFreq);

                // Lateral harmonic sway & crampon balance instability
                float swayExcite = (isSprinting ? 0.065f : 0.025f) * (playerHorizSpeed / 1.5f);
                ladder.swayVelocity += swayExcite * std::sin(m_simTime * 2.0f * 3.14159f * (oscFreq * 0.75f));
                ladder.swayVelocity -= ladder.swayVelocity * 4.5f * deltaTime; // Damping
                ladder.swayAngleRadians += ladder.swayVelocity * deltaTime;
                ladder.swayAngleRadians = std::clamp(ladder.swayAngleRadians, -0.22f, 0.22f); // Max ~12.6 degrees

                // Calculate supported deck height
                float baseHeight = ladder.anchorA.y + s * (ladder.anchorB.y - ladder.anchorA.y);
                float totalDeflect = ladder.currentDeflectionMeters + ladder.verticalOscillationM;
                float supportedY = baseHeight - totalDeflect;

                // Rung stepping detection: trigger metallic clink every rung (0.30m spacing)
                int currentRung = static_cast<int>(std::floor(s * static_cast<float>(ladder.numRungs)));
                if (currentRung != ladder.lastRungIndex && currentRung >= 0 && currentRung < static_cast<int>(ladder.numRungs)) {
                    ladder.lastRungIndex = currentRung;
                    result.triggeredRungStep = true;
                    result.rungIndex = currentRung;
                    result.rungWorldPos = ladder.anchorA + u * (s * spanLen);
                }

                result.isOnStructure = true;
                result.isLadder = true;
                result.supportedHeightY = supportedY;
                result.lateralSwayRollRad = ladder.swayAngleRadians;
                result.deflectionMeters = totalDeflect;
                return result;
            }
        }
    }

    // =========================================================================
    // 2. Resolve Sintered Firn Snow Bridges
    // =========================================================================
    for (size_t i = 0; i < m_snowBridges.size(); i++) {
        auto& bridge = m_snowBridges[i];
        if (bridge.status == BridgeStatus::Destroyed) continue;

        glm::vec3 spanVec = bridge.anchorB - bridge.anchorA;
        float spanLen = glm::length(spanVec);
        if (spanLen < 0.5f) continue;
        glm::vec3 u = spanVec / spanLen;

        glm::vec3 toPlayer = playerPos - bridge.anchorA;
        float progress = glm::dot(toPlayer, u) / spanLen;

        if (progress >= -0.05f && progress <= 1.05f) {
            glm::vec3 centerlinePt = bridge.anchorA + u * (progress * spanLen);
            glm::vec3 lateralOffset = playerPos - centerlinePt;
            float distLateral = glm::length(lateralOffset);

            if (distLateral <= bridge.width * 0.5f) {
                m_activeBridgeIndex = static_cast<int>(i);
                float s = std::clamp(progress, 0.0f, 1.0f);
                float x = s * spanLen;

                // Self-weight bending moment: M_self = w * x * (L - x) / 2
                float selfWeightW = bridge.densityKgM3 * g * bridge.width * bridge.thickness;
                float mSelf = (selfWeightW * x * (spanLen - x)) * 0.5f;

                // Dynamic player point load bending moment: M_player = F * x * (L - x) / L
                float mPlayer = (effectiveLoadForceN * x * (spanLen - x)) / spanLen;

                // Section modulus Z = W * h^2 / 6
                float Z = (bridge.width * bridge.thickness * bridge.thickness) / 6.0f;
                float totalBendingStressPa = (mSelf + mPlayer) / std::max(0.001f, Z);
                bridge.currentStressPa = totalBendingStressPa;

                // Structural failure check against critical firn tensile strength
                if (totalBendingStressPa > bridge.criticalStrengthPa) {
                    if (bridge.status != BridgeStatus::Collapsing) {
                        bridge.status = BridgeStatus::Collapsing;
                        bridge.collapseTimer = 0.0f;
                        result.triggeredCollapse = true;
                        result.collapseWorldPos = bridge.getCenter();
                        result.chasmDepth = bridge.chasmDepthMeters;

                        std::cout << "[CrevasseBridgeSystem] CRITICAL STRUCTURAL FAILURE! Bending stress "
                                  << std::fixed << std::setprecision(1) << (totalBendingStressPa / 1000.0f)
                                  << " kPa exceeded critical strength " << (bridge.criticalStrengthPa / 1000.0f)
                                  << " kPa! Arch collapsing into " << bridge.chasmDepthMeters << "m crevasse!"
                                  << std::endl;
                    }
                } else if (totalBendingStressPa > bridge.criticalStrengthPa * 0.68f) {
                    if (bridge.status == BridgeStatus::Intact) {
                        bridge.status = BridgeStatus::Creaking;
                        result.triggeredCrack = true;
                        result.collapseWorldPos = playerPos;
                    }
                }

                // Supported surface calculation
                float baseHeight = bridge.anchorA.y + s * (bridge.anchorB.y - bridge.anchorA.y);
                float archSag = bridge.currentSagMeters * 4.0f * s * (1.0f - s);

                if (bridge.status == BridgeStatus::Collapsing) {
                    // Free fall collapse acceleration
                    float collapseDrop = 0.5f * g * bridge.collapseTimer * bridge.collapseTimer * 2.0f;
                    result.supportedHeightY = baseHeight - archSag - collapseDrop;
                } else {
                    result.supportedHeightY = baseHeight - archSag;
                }

                result.isOnStructure = true;
                result.isLadder = false;
                result.deflectionMeters = archSag;
                result.chasmDepth = bridge.chasmDepthMeters;
                return result;
            }
        }
    }

    return result;
}

void CrevasseBridgeSystem::triggerBridgeCollapseById(const std::string& bridgeId) {
    for (auto& bridge : m_snowBridges) {
        if (bridge.id == bridgeId || bridgeId == "all") {
            bridge.status = BridgeStatus::Collapsing;
            bridge.collapseTimer = 0.0f;
            std::cout << "[CrevasseBridgeSystem] Manual trigger: " << bridge.locationName << " collapsing!" << std::endl;
        }
    }
}

float CrevasseBridgeSystem::getActiveLadderSway() const {
    if (m_activeLadderIndex >= 0 && m_activeLadderIndex < static_cast<int>(m_ladders.size())) {
        return m_ladders[m_activeLadderIndex].swayAngleRadians;
    }
    return 0.0f;
}

float CrevasseBridgeSystem::getActiveLadderDeflection() const {
    if (m_activeLadderIndex >= 0 && m_activeLadderIndex < static_cast<int>(m_ladders.size())) {
        return m_ladders[m_activeLadderIndex].currentDeflectionMeters + m_ladders[m_activeLadderIndex].verticalOscillationM;
    }
    return 0.0f;
}

std::string CrevasseBridgeSystem::getActiveStructureTelemetry() const {
    std::stringstream ss;
    if (m_activeLadderIndex >= 0) {
        const auto& ladder = m_ladders[m_activeLadderIndex];
        ss << "Ladder: " << ladder.locationName
           << " | Span: " << std::fixed << std::setprecision(1) << ladder.getSpanLength() << "m"
           << " | Deflection: " << std::setprecision(2) << (ladder.currentDeflectionMeters * 100.0f) << "cm"
           << " | Sway: " << std::setprecision(1) << glm::degrees(ladder.swayAngleRadians) << "°";
    } else if (m_activeBridgeIndex >= 0) {
        const auto& bridge = m_snowBridges[m_activeBridgeIndex];
        std::string statusStr = (bridge.status == BridgeStatus::Intact) ? "Intact" :
                                (bridge.status == BridgeStatus::Creaking) ? "CREAKING / COMPROMISED" :
                                (bridge.status == BridgeStatus::Collapsing) ? "COLLAPSING!" : "DESTROYED";
        ss << "Snowbridge: " << bridge.locationName
           << " | Status: " << statusStr
           << " | Stress: " << std::fixed << std::setprecision(1) << (bridge.currentStressPa / 1000.0f) << " kPa"
           << " / " << (bridge.criticalStrengthPa / 1000.0f) << " kPa";
    }
    return ss.str();
}

} // namespace whiteout::game
