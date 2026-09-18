#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>

namespace whiteout::game {

class TerrainCollider;
class WeatherSystem;

// Avalanche Hazard Rating (European / North American Avalanche Danger Scale)
enum class AvalancheHazardLevel {
    Low = 1,        // Gering (S > 2.0)
    Moderate = 2,   // Mäßig (1.5 < S <= 2.0)
    Considerable = 3,// Erheblich (1.0 < S <= 1.5, human triggered slabs common)
    High = 4,       // Groß (0.75 < S <= 1.0, widespread spontaneous slabs)
    VeryHigh = 5    // Sehr groß (S <= 0.75, catastrophic major avalanches)
};

// Stratified Snowpack Layer Physical Properties (SLF Davos / Gaume Continuum Model)
struct SnowpackStratigraphy {
    // 1. Cohesive Surface Wind Slab (Triebschneeplatte)
    float slabThicknessMeters = 0.65f;        // 0.2m - 1.4m
    float slabDensityKgM3 = 280.0f;            // 200 - 360 kg/m^3
    float youngModulusMPa = 22.0f;             // 10 - 35 MPa (tensile elasticity)
    float tensileStrengthKPa = 14.5f;          // 6 - 22 kPa (cohesive tensile limit)

    // 2. Buried Weak Layer (Schwimmschnee / Tiefenreif / Depth Hoar)
    float weakLayerThicknessMeters = 0.035f;   // 0.01m - 0.06m
    float weakLayerShearStrengthPa = 980.0f;   // Critical shear strength (tau_c: 600 - 1500 Pa)
    float criticalFractureEnergyJ = 0.85f;     // Specific fracture energy (G_c: 0.2 - 2.0 J/m^2)
    float collapseAmplitudeMeters = 0.025f;    // Anticrack volumetric compaction (m)

    // 3. Bed Surface (Altschneefundament / Felsbett)
    float bedSurfaceFriction = 0.42f;          // Coulomb sliding friction coefficient (mu)
    float firnHardnessFactor = 0.88f;          // Compaction of bed surface
};

// Crown Fracture Line Event (Anrisskante)
struct CrownFractureEvent {
    bool active = false;
    glm::vec3 originWorldPos{0.0f};           // Epicenter of weak-layer collapse
    glm::vec3 crackNormal{0.0f, 1.0f, 0.0f};  // Slope surface normal
    glm::vec3 propagationDir{1.0f, 0.0f, 0.0f};// Lateral crack direction across slope
    float currentRadiusMeters = 0.0f;         // Propagating crack front radius
    float maxRadiusMeters = 180.0f;           // Total crown fracture breadth across face
    float propagationSpeedMps = 45.0f;         // Fast anticrack propagation (30 - 80 m/s)
    float slabStepHeightMeters = 0.75f;       // Vertical fracture cliff step
    float timeSec = 0.0f;
    float durationSec = 12.0f;
    std::vector<glm::vec3> fracturePath;      // Jagged polyline contour of the Anrisskante
};

// Dynamic Snow Creep & Wind Drift State
struct SnowpackDynamics {
    float creepVelocityMmPerHour = 0.0f;      // Downhill visco-elastic crawl
    float cumulativeCreepMeters = 0.0f;       // Total creep displacement
    float windDriftRateCmPerHour = 0.0f;      // Leeward accumulation / windward erosion
    float leewardSlabDepthMeters = 0.0f;      // Local Triebschneelinse thickness
    float stabilityIndexS = 2.2f;             // S = tau_c / tau_shear (< 1.0 = CRITICAL FAILURE)
    AvalancheHazardLevel hazardLevel = AvalancheHazardLevel::Low;
    bool weakLayerCollapsed = false;
};

class SnowpackSimulation {
public:
    explicit SnowpackSimulation(const TerrainCollider& collider);
    ~SnowpackSimulation() = default;

    void update(
        float deltaTime,
        const glm::vec3& playerPos,
        float playerSpeed,
        float slopeDegrees,
        bool playerGrounded,
        const WeatherSystem& weather
    );

    // Trigger weak layer collapse and crown fracture propagation
    void triggerSlabFracture(const glm::vec3& epicenter, float slabDepth = 0.85f);

    // Callbacks for audio/visual synchronization
    using WhumpfCallback = std::function<void(const glm::vec3& pos, float intensity)>;
    using CrownSnapCallback = std::function<void(const glm::vec3& pos, float crackLength)>;
    using AvalancheReleaseCallback = std::function<void(const glm::vec3& pos, const std::vector<glm::vec3>& crownPath)>;

    void setWhumpfCallback(WhumpfCallback cb) { m_onWhumpf = std::move(cb); }
    void setCrownSnapCallback(CrownSnapCallback cb) { m_onCrownSnap = std::move(cb); }
    void setAvalancheReleaseCallback(AvalancheReleaseCallback cb) { m_onAvalancheRelease = std::move(cb); }

    [[nodiscard]] const SnowpackStratigraphy& getStratigraphy() const { return m_stratigraphy; }
    [[nodiscard]] const SnowpackDynamics& getDynamics() const { return m_dynamics; }
    [[nodiscard]] const CrownFractureEvent& getActiveCrownFracture() const { return m_crownFracture; }
    [[nodiscard]] bool isCrownFractureActive() const { return m_crownFracture.active; }

    // Real-time telemetry for HUD and diagnostic inspections
    [[nodiscard]] std::string getSnowpackTelemetry() const;

private:
    void updateStratigraphyAndCreep(float deltaTime, float slopeDegrees, const WeatherSystem& weather);
    void updateStabilityAndSurcharge(
        const glm::vec3& playerPos,
        float playerSpeed,
        float slopeDegrees,
        bool playerGrounded
    );
    void updateCrownFracture(float deltaTime);

    const TerrainCollider& m_collider;
    SnowpackStratigraphy m_stratigraphy;
    SnowpackDynamics m_dynamics;
    CrownFractureEvent m_crownFracture;

    // Simulation timing and state
    float m_simTime = 0.0f;
    float m_whumpfCooldown = 0.0f;
    float m_windDriftTimer = 0.0f;
    glm::vec3 m_lastPlayerPos{0.0f};

    // Event hooks
    WhumpfCallback m_onWhumpf;
    CrownSnapCallback m_onCrownSnap;
    AvalancheReleaseCallback m_onAvalancheRelease;
};

} // namespace whiteout::game
