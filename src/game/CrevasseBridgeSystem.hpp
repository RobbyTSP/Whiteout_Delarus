#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>

namespace whiteout::game {

// =============================================================================
// Snow Bridge Structural Failure Status
// =============================================================================
enum class BridgeStatus {
    Intact,       // Cohesive firn arch supporting self-weight
    Creaking,     // Tensile micro-cracks forming under mountaineer surcharge
    Collapsing,   // Shear punch-through & arch collapse in progress
    Destroyed     // Fractured void dropping into the abyssal crevasse
};

struct SnowBridge {
    std::string id;
    std::string locationName;
    glm::vec3 anchorA;          // Near rim anchor point
    glm::vec3 anchorB;          // Far rim anchor point
    float width = 2.2f;         // Lateral width of the firn arch (m)
    float thickness = 0.65f;    // Arch thickness at crown (m)
    float densityKgM3 = 380.0f; // Sintered firn density (kg/m^3)
    float criticalStrengthPa = 34000.0f; // Ultimate bending/tensile failure stress (Pa)
    float currentSagMeters = 0.08f;      // Accumulated visco-elastic creep sag (m)
    float currentStressPa = 0.0f;        // Active bending stress under load (Pa)
    float collapseTimer = 0.0f;          // Time elapsed since collapse initiated
    float chasmDepthMeters = 24.0f;      // Vertical drop to crevasse floor (m)
    BridgeStatus status = BridgeStatus::Intact;

    [[nodiscard]] float getSpanLength() const;
    [[nodiscard]] glm::vec3 getCenter() const;
    [[nodiscard]] glm::vec3 getSpanDirection() const;
};

// =============================================================================
// 4-Section Sectional Aluminum Crevasse Ladder (Khumbu Icefall Specification)
// =============================================================================
struct AluminumLadder {
    std::string id;
    std::string locationName;
    glm::vec3 anchorA;          // Ledge A position (m)
    glm::vec3 anchorB;          // Ledge B position (m)
    float width = 0.48f;        // Rung width (m)
    uint32_t numSections = 4;   // 4 interlocking sections tied with climbing webbing
    uint32_t numRungs = 28;     // Rungs spaced every ~0.30m
    float elasticModulusPa = 69e9f; // 6061-T6 aluminum (69 GPa)
    float momentOfInertiaM4 = 3.6e-7f; // Combined 2-rail second moment of area
    float currentDeflectionMeters = 0.0f; // Midspan dynamic deflection (Euler-Bernoulli)
    float swayAngleRadians = 0.0f;        // Harmonic lateral roll sway
    float swayVelocity = 0.0f;
    float verticalOscillationM = 0.0f;
    float naturalFrequencyHz = 2.85f;
    bool playerOnLadder = false;
    float playerProgress = 0.0f; // 0.0 at anchorA, 1.0 at anchorB
    int lastRungIndex = -1;

    [[nodiscard]] float getSpanLength() const;
    [[nodiscard]] glm::vec3 getCenter() const;
    [[nodiscard]] glm::vec3 getSpanDirection() const;
};

// =============================================================================
// Bergschrund Fissure (Tectonic Glacier Headwall Chasm)
// =============================================================================
struct BergschrundFissure {
    std::string id;
    std::string locationName;
    glm::vec3 rimUpper;         // Stagnant headwall/rim face
    glm::vec3 rimLower;         // Downstream flowing glacier ice lip
    float gapWidthMeters = 5.2f;
    float verticalDropMeters = 4.8f;
    float depthMeters = 32.0f;
    float openingVelocityMmPerDay = 1.8f; // Extensional flow gaping
};

// Interaction result for player character movement & collision
struct BridgeInteractionResult {
    bool isOnStructure = false;
    bool isLadder = false;
    float supportedHeightY = 0.0f;
    float lateralSwayRollRad = 0.0f;
    float deflectionMeters = 0.0f;
    bool triggeredRungStep = false;
    int rungIndex = -1;
    glm::vec3 rungWorldPos{0.0f};
    bool triggeredCrack = false;
    bool triggeredCollapse = false;
    glm::vec3 collapseWorldPos{0.0f};
    float chasmDepth = 25.0f;
};

class CrevasseBridgeSystem {
public:
    CrevasseBridgeSystem();

    void initDefaultExpeditionStructures();
    void update(float deltaTime, float ambientTempC = -15.0f);

    // Resolve player walking on snow bridges or aluminum ladders
    BridgeInteractionResult resolvePlayer(
        const glm::vec3& playerPos,
        const glm::vec3& playerVelocity,
        bool isSprinting,
        bool isJumpingOrLanding,
        float mountaineerMassKg,
        float deltaTime
    );

    // Force trigger collapse for testing / demo
    void triggerBridgeCollapseById(const std::string& bridgeId);

    [[nodiscard]] const std::vector<SnowBridge>& getSnowBridges() const { return m_snowBridges; }
    [[nodiscard]] const std::vector<AluminumLadder>& getLadders() const { return m_ladders; }
    [[nodiscard]] const std::vector<BergschrundFissure>& getBergschrunds() const { return m_bergschrunds; }

    [[nodiscard]] bool isPlayerOnLadder() const { return m_activeLadderIndex >= 0; }
    [[nodiscard]] float getActiveLadderSway() const;
    [[nodiscard]] float getActiveLadderDeflection() const;
    [[nodiscard]] std::string getActiveStructureTelemetry() const;

private:
    std::vector<SnowBridge> m_snowBridges;
    std::vector<AluminumLadder> m_ladders;
    std::vector<BergschrundFissure> m_bergschrunds;

    int m_activeBridgeIndex = -1;
    int m_activeLadderIndex = -1;
    float m_simTime = 0.0f;
};

} // namespace whiteout::game
