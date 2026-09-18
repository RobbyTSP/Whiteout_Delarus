#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <SDL2/SDL.h>
#include "AcousticRaytracer.hpp"
#include "../game/TerrainCollider.hpp"
#include "../game/Player.hpp"
#include "../game/WeatherSystem.hpp"
#include "../core/Camera.hpp"

namespace whiteout::audio {

// Real-time Biquad Filter for acoustic DSP
class BiquadFilter {
public:
    BiquadFilter() = default;

    void setLowpass(float sampleRate, float cutoffHz, float q = 0.7071f);
    void setHighpass(float sampleRate, float cutoffHz, float q = 0.7071f);
    void setBandpass(float sampleRate, float centerHz, float q = 1.0f);
    float process(float in);
    void reset();

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

// Acoustic Ring Buffer Delay Line for 35km Mountain Echoes (up to 8 seconds delay)
class AcousticDelayLine {
public:
    explicit AcousticDelayLine(float maxDelaySeconds = 8.0f, int sampleRate = 48000);

    void write(float sampleL, float sampleR);
    void read(float delaySeconds, float& outL, float& outR) const;
    void clear();

private:
    std::vector<float> m_bufferL;
    std::vector<float> m_bufferR;
    size_t m_writeIndex = 0;
    size_t m_capacity = 0;
    int m_sampleRate = 48000;
};

// Physical Footstep Audio Voice
struct FootstepVoice {
    bool active = false;
    game::AlpineSurfaceType surfaceType = game::AlpineSurfaceType::HardFirnSnow;
    float intensity = 1.0f;
    float timeSec = 0.0f;
    float durationSec = 0.25f;
    float pan = 0.0f;
    BiquadFilter filterL;
    BiquadFilter filterR;
};

// Infrasound Sérac Fracture & Avalanche Event
struct IceCollapseEvent {
    bool active = false;
    glm::vec3 worldPos{0.0f};
    float magnitude = 1.0f;
    float timeSec = 0.0f;
    float durationSec = 6.0f;
    float directDelaySec = 0.0f;
    float nuptseEchoDelaySec = 4.6f;
    float pan = 0.0f;
};

class AlpineAudioEngine {
public:
    AlpineAudioEngine();
    ~AlpineAudioEngine();

    bool init(const game::TerrainCollider* collider);
    void shutdown();

    // Per-frame state update from main game loop
    void update(
        float deltaTime,
        const core::Camera& camera,
        const game::Player& player,
        const game::WeatherSystem& weather
    );

    // Event triggers
    void triggerFootstep(game::AlpineSurfaceType surface, float intensity = 1.0f, bool isLeftFoot = false);
    void triggerIcefallCollapse(const glm::vec3& worldPos, float magnitude = 1.0f);
    void triggerAvalanche(float intensity = 1.0f);

    // Audio offline renderer to WAV file (48kHz 16-bit stereo) for testing & verification
    bool renderToWav(
        const std::string& filepath,
        float durationSeconds,
        const core::Camera& camera,
        const game::Player& player,
        const game::WeatherSystem& weather
    );

    [[nodiscard]] bool isInitialized() const { return m_initialized; }
    [[nodiscard]] const AcousticEnvironment& getCurrentAcoustics() const { return m_currentEnvironment; }
    [[nodiscard]] std::string getAudioTelemetry() const;

    // SDL audio streaming callback
    void fillAudioBuffer(float* stream, int numFrames);

private:
    void generateAudioFrame(float& left, float& right, float dt);

    bool m_initialized = false;
    SDL_AudioDeviceID m_audioDevice = 0;
    int m_sampleRate = 48000;

    std::unique_ptr<AcousticRaytracer> m_raytracer;
    AcousticEnvironment m_currentEnvironment;
    float m_raytraceTimer = 0.0f;

    std::mutex m_audioMutex;

    // Simulated Environmental State
    glm::vec3 m_listenerPos{0.0f};
    glm::vec3 m_listenerForward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_listenerRight{1.0f, 0.0f, 0.0f};
    float m_windSpeedKmh = 30.0f;
    float m_blizzardFactor = 0.0f;
    float m_windDirectionDeg = 135.0f;
    float m_altitude = 5300.0f;

    // Player Physiology
    float m_heartRateBpm = 75.0f;
    float m_heartbeatPulse = 0.0f;
    float m_hypoxiaFactor = 0.0f;
    float m_exertion = 0.2f;
    float m_breathPhase = 0.0f;
    bool m_gogglesEquipped = true;

    // DSP Generators & Filters
    uint32_t m_noiseSeed = 123456789u;
    float m_pinkB0 = 0.0f, m_pinkB1 = 0.0f, m_pinkB2 = 0.0f;
    float m_pinkB3 = 0.0f, m_pinkB4 = 0.0f, m_pinkB5 = 0.0f, m_pinkB6 = 0.0f;

    // Wind Filters
    BiquadFilter m_windRumbleFilter;
    BiquadFilter m_windVortexFilter;
    BiquadFilter m_windSpindriftFilter;

    // Raytraced Mountain Echo Delay Line
    AcousticDelayLine m_echoDelayLine;
    BiquadFilter m_nuptseEchoFilterL;
    BiquadFilter m_nuptseEchoFilterR;

    // Footstep Voice Pool
    static constexpr size_t MAX_FOOTSTEP_VOICES = 6;
    std::vector<FootstepVoice> m_footstepVoices;

    // Active Icefall Calving / Avalanche Events
    std::vector<IceCollapseEvent> m_iceCollapses;

    // Periodic ambient ice cracking timer
    float m_ambientCrackTimer = 5.0f;

    // Helper functions
    float getWhiteNoise();
    float getPinkNoise();
};

} // namespace whiteout::audio
