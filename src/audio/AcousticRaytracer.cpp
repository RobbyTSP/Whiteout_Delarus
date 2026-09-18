#include "AcousticRaytracer.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace whiteout::audio {

AcousticRaytracer::AcousticRaytracer(const game::TerrainCollider& collider)
    : m_collider(collider) {
}

float AcousticRaytracer::getSpeedOfSound(float altitudeMeters, float temperatureCelsius) const {
    // Adiabatic acoustic wave velocity in air: c = 331.3 * sqrt(1 + T / 273.15)
    float tKelvin = std::max(200.0f, 273.15f + temperatureCelsius);
    float cBase = 331.3f * std::sqrt(tKelvin / 273.15f);

    // Rarefied high-altitude pressure correction (negligible dispersion, slight molecular relaxation change)
    float altCorrection = std::clamp(1.0f - (altitudeMeters / 8848.86f) * 0.025f, 0.95f, 1.0f);
    return cBase * altCorrection;
}

AcousticEnvironment AcousticRaytracer::traceEnvironment(
    const glm::vec3& listenerPos,
    const glm::vec3& listenerForward,
    const glm::vec3& listenerRight
) {
    AcousticEnvironment env{};
    float c = getSpeedOfSound(listenerPos.y, -22.0f);

    // 1. Radial raycasting: 16 rays covering 360 degrees around listener
    const int numRays = 16;
    float totalEnclosure = 0.0f;
    float maxElevationDifference = 0.0f;
    int wallHits = 0;

    for (int i = 0; i < numRays; i++) {
        float azimuth = static_cast<float>(i) * (6.2831853f / static_cast<float>(numRays));
        glm::vec3 rayDir(std::cos(azimuth), 0.05f, std::sin(azimuth)); // slightly elevated ray
        rayDir = glm::normalize(rayDir);

        // Raymarch across DEM
        float stepSize = 40.0f;
        float maxDist = 5500.0f;
        float currentDist = 80.0f;

        while (currentDist < maxDist) {
            glm::vec3 samplePos = listenerPos + rayDir * currentDist;
            float terrainH = m_collider.getHeight(samplePos.x, samplePos.z);

            // Did the acoustic ray collide with elevated terrain / mountain wall?
            if (samplePos.y <= terrainH) {
                float slope = m_collider.getSlopeAngleDegrees(samplePos.x, samplePos.z);
                glm::vec3 normal = m_collider.getNormal(samplePos.x, samplePos.z);
                auto geology = m_collider.getGeologyInfo(samplePos.x, samplePos.z);

                float elevDiff = terrainH - listenerPos.y;
                if (elevDiff > 150.0f) {
                    totalEnclosure += 1.0f;
                    maxElevationDifference = std::max(maxElevationDifference, elevDiff);
                }

                // Steep terrain faces (> 30°) act as monumental specular reflectors
                if (slope >= 30.0f && currentDist > 200.0f) {
                    float delay = (currentDist * 2.0f) / c; // Round-trip propagation delay

                    // Specular reflection factor: faces directly opposing sound wave reflect most acoustic energy
                    float incidenceCos = std::max(0.15f, -glm::dot(normal, rayDir));

                    // Material-specific acoustic reflectance and lowpass frequency absorption
                    float rCoeff = 0.50f * incidenceCos;
                    float cutoffHz = 1800.0f;

                    switch (geology.surfaceType) {
                        case game::AlpineSurfaceType::ExposedRockFace:
                            rCoeff = 0.94f; // Massive granite / limestone: highly reflective
                            cutoffHz = 3800.0f;
                            break;
                        case game::AlpineSurfaceType::GlacialBlueIce:
                            rCoeff = 0.88f; // Hard metamorphic blue ice: crisp slapback
                            cutoffHz = 4200.0f;
                            break;
                        case game::AlpineSurfaceType::HardFirnSnow:
                            rCoeff = 0.35f; // Sintered firn: absorbs high frequencies
                            cutoffHz = 850.0f;
                            break;
                        case game::AlpineSurfaceType::TalusScreeSlope:
                            rCoeff = 0.55f; // Loose gravel / scree
                            cutoffHz = 1500.0f;
                            break;
                    }

                    // Geometric 1/r spreading & dry alpine atmospheric absorption
                    float geomSpread = 1.0f / (1.0f + std::pow(currentDist / 200.0f, 1.25f));
                    float airAbsorption = std::exp(-0.00018f * currentDist);
                    float gain = std::clamp(rCoeff * geomSpread * airAbsorption, 0.0f, 0.95f);

                    // Stereo panning relative to listener head orientation
                    glm::vec3 toWall = glm::normalize(samplePos - listenerPos);
                    float pan = std::clamp(glm::dot(toWall, listenerRight), -1.0f, 1.0f);

                    // Identify geographical landmark reflectors
                    std::string wallName = "Alpine Mountain Face";
                    if (samplePos.x > -12500.0f && samplePos.x < -9500.0f && samplePos.z > -6800.0f && samplePos.z < -4800.0f) {
                        wallName = "Nuptse 3,000m Granite Wall (South Face)";
                    } else if (samplePos.x > -8500.0f && samplePos.x < -7200.0f && samplePos.z > -5600.0f && samplePos.z < -4500.0f) {
                        wallName = "Lhotse 1,100m Ice Face";
                    } else if (samplePos.x > -9200.0f && samplePos.x < -8000.0f && samplePos.z > -8600.0f && samplePos.z < -7500.0f) {
                        wallName = "Everest Southwest Face";
                    }

                    if (gain > 0.015f && env.echoTaps.size() < 8) {
                        AcousticEchoTap tap{};
                        tap.delaySeconds = delay;
                        tap.gain = gain;
                        tap.stereoPan = pan;
                        tap.lowpassCutoffHz = cutoffHz;
                        tap.reflectorElevation = terrainH;
                        tap.reflectorName = wallName;
                        env.echoTaps.push_back(tap);
                    }
                    wallHits++;
                }
                break;
            }
            currentDist += stepSize;
        }
    }

    // 2. Valley Enclosure & Reverberation Time T60
    env.valleyEnclosure = std::clamp(totalEnclosure / static_cast<float>(numRays), 0.0f, 1.0f);

    // In open summits: RT60 ~ 0.5s - 1.2s.
    // In deep glacial cirques (Western Cwm / Nuptse amphitheater): RT60 ~ 3.5s - 6.0s!
    env.reverberationTimeT60 = 0.8f + env.valleyEnclosure * 4.2f;

    // 3. Knife-Edge Ridge Wind Exposure
    // Sample terrain drops at 80m left and right
    float hLeft = m_collider.getHeight(listenerPos.x - listenerRight.x * 80.0f, listenerPos.z - listenerRight.z * 80.0f);
    float hRight = m_collider.getHeight(listenerPos.x + listenerRight.x * 80.0f, listenerPos.z + listenerRight.z * 80.0f);
    float dropLeft = std::max(0.0f, listenerPos.y - hLeft);
    float dropRight = std::max(0.0f, listenerPos.y - hRight);

    // If both sides drop off steeply (e.g. Hillary Step / Summit Ridge), exposure is high
    float ridgeDrop = (dropLeft + dropRight) * 0.5f;
    env.ridgeWindExposure = std::clamp(ridgeDrop / 75.0f, 0.1f, 1.0f);

    return env;
}

std::vector<AcousticEchoTap> AcousticRaytracer::traceSourceToListener(
    const glm::vec3& sourcePos,
    const glm::vec3& listenerPos,
    const glm::vec3& listenerRight
) {
    std::vector<AcousticEchoTap> taps;
    float c = getSpeedOfSound((sourcePos.y + listenerPos.y) * 0.5f, -22.0f);

    // 1. Direct line-of-sight sound propagation
    float directDist = glm::length(sourcePos - listenerPos);
    float directDelay = directDist / c;
    float directGain = 1.0f / (1.0f + std::pow(directDist / 120.0f, 1.15f));
    float directPan = glm::dot(glm::normalize(sourcePos - listenerPos), listenerRight);

    AcousticEchoTap directTap{};
    directTap.delaySeconds = directDelay;
    directTap.gain = std::clamp(directGain, 0.0f, 1.0f);
    directTap.stereoPan = std::clamp(directPan, -1.0f, 1.0f);
    directTap.lowpassCutoffHz = std::max(800.0f, 12000.0f * std::exp(-0.0003f * directDist));
    directTap.reflectorElevation = sourcePos.y;
    directTap.reflectorName = "Direct Sound Path";
    taps.push_back(directTap);

    // 2. Primary Nuptse Wall Specular Reflection Path
    // Nuptse center of mass: ~ (-11000, 7200, -5800)
    glm::vec3 nuptseWallCenter(-11000.0f, 7200.0f, -5800.0f);
    float dSourceNuptse = glm::length(sourcePos - nuptseWallCenter);
    float dNuptseListener = glm::length(listenerPos - nuptseWallCenter);
    float echoDist = dSourceNuptse + dNuptseListener;
    float echoDelay = echoDist / c;

    float echoGain = 0.92f * (1.0f / (1.0f + std::pow(echoDist / 250.0f, 1.3f))) * std::exp(-0.00015f * echoDist);
    float echoPan = glm::dot(glm::normalize(nuptseWallCenter - listenerPos), listenerRight);

    AcousticEchoTap nuptseTap{};
    nuptseTap.delaySeconds = echoDelay;
    nuptseTap.gain = std::clamp(echoGain * 2.2f, 0.0f, 0.85f);
    nuptseTap.stereoPan = std::clamp(echoPan, -1.0f, 1.0f);
    nuptseTap.lowpassCutoffHz = 2800.0f;
    nuptseTap.reflectorElevation = 7861.0f;
    nuptseTap.reflectorName = "Nuptse 3,000m Granite Wall Echo";
    taps.push_back(nuptseTap);

    return taps;
}

} // namespace whiteout::audio
