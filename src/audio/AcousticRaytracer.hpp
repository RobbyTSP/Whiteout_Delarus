#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "../game/TerrainCollider.hpp"

namespace whiteout::audio {

struct AcousticEchoTap {
    float delaySeconds = 0.0f;       // Acoustic propagation delay (e.g. 0.5s to 8.0s)
    float gain = 0.0f;               // Attenuated amplitude [0..1]
    float stereoPan = 0.0f;          // -1.0 (left) to +1.0 (right)
    float lowpassCutoffHz = 2000.0f; // Material & distance low-pass absorption cutoff
    float reflectorElevation = 0.0f; // Elevation of reflecting terrain feature (meters)
    std::string reflectorName;       // e.g. "Nuptse 3,000m Granite South Face"
};

struct AcousticEnvironment {
    float reverberationTimeT60 = 2.8f; // RT60 reverberation decay in seconds
    float ridgeWindExposure = 0.3f;    // 0.0 (sheltered basin) to 1.0 (exposed knife-edge ridge)
    float valleyEnclosure = 0.5f;      // 0.0 (flat plain) to 1.0 (deep glacial amphitheater)
    std::vector<AcousticEchoTap> echoTaps;
};

class AcousticRaytracer {
public:
    explicit AcousticRaytracer(const game::TerrainCollider& collider);

    // Traces acoustic rays across the 35km DEM to evaluate echoes, reflection delays, and exposure
    AcousticEnvironment traceEnvironment(const glm::vec3& listenerPos, const glm::vec3& listenerForward, const glm::vec3& listenerRight);

    // Traces direct propagation and reflection path between a sound source (e.g. ice crack) and listener
    std::vector<AcousticEchoTap> traceSourceToListener(
        const glm::vec3& sourcePos,
        const glm::vec3& listenerPos,
        const glm::vec3& listenerRight
    );

    [[nodiscard]] float getSpeedOfSound(float altitudeMeters, float temperatureCelsius) const;

private:
    const game::TerrainCollider& m_collider;
    float m_baseSpeedOfSound = 320.0f; // m/s at -20°C high altitude
};

} // namespace whiteout::audio
