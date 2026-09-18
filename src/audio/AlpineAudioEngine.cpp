#include "AlpineAudioEngine.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace whiteout::audio {

// =============================================================================
// BiquadFilter Implementation
// =============================================================================
void BiquadFilter::setLowpass(float sampleRate, float cutoffHz, float q) {
    cutoffHz = std::clamp(cutoffHz, 20.0f, sampleRate * 0.49f);
    float w0 = 6.2831853f * (cutoffHz / sampleRate);
    float alpha = std::sin(w0) / (2.0f * q);
    float cosw0 = std::cos(w0);

    float a0 = 1.0f + alpha;
    b0 = ((1.0f - cosw0) * 0.5f) / a0;
    b1 = (1.0f - cosw0) / a0;
    b2 = ((1.0f - cosw0) * 0.5f) / a0;
    a1 = (-2.0f * cosw0) / a0;
    a2 = (1.0f - alpha) / a0;
}

void BiquadFilter::setHighpass(float sampleRate, float cutoffHz, float q) {
    cutoffHz = std::clamp(cutoffHz, 20.0f, sampleRate * 0.49f);
    float w0 = 6.2831853f * (cutoffHz / sampleRate);
    float alpha = std::sin(w0) / (2.0f * q);
    float cosw0 = std::cos(w0);

    float a0 = 1.0f + alpha;
    b0 = ((1.0f + cosw0) * 0.5f) / a0;
    b1 = (-(1.0f + cosw0)) / a0;
    b2 = ((1.0f + cosw0) * 0.5f) / a0;
    a1 = (-2.0f * cosw0) / a0;
    a2 = (1.0f - alpha) / a0;
}

void BiquadFilter::setBandpass(float sampleRate, float centerHz, float q) {
    centerHz = std::clamp(centerHz, 20.0f, sampleRate * 0.49f);
    float w0 = 6.2831853f * (centerHz / sampleRate);
    float alpha = std::sin(w0) / (2.0f * q);
    float cosw0 = std::cos(w0);

    float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;
    a1 = (-2.0f * cosw0) / a0;
    a2 = (1.0f - alpha) / a0;
}

float BiquadFilter::process(float in) {
    float out = b0 * in + z1;
    z1 = b1 * in - a1 * out + z2;
    z2 = b2 * in - a2 * out;
    return out;
}

void BiquadFilter::reset() {
    z1 = 0.0f;
    z2 = 0.0f;
}

// =============================================================================
// AcousticDelayLine Implementation (Circular Buffer for Mountain Echoes)
// =============================================================================
AcousticDelayLine::AcousticDelayLine(float maxDelaySeconds, int sampleRate)
    : m_sampleRate(sampleRate) {
    m_capacity = static_cast<size_t>(maxDelaySeconds * static_cast<float>(sampleRate)) + 128;
    m_bufferL.assign(m_capacity, 0.0f);
    m_bufferR.assign(m_capacity, 0.0f);
    m_writeIndex = 0;
}

void AcousticDelayLine::write(float sampleL, float sampleR) {
    m_bufferL[m_writeIndex] = sampleL;
    m_bufferR[m_writeIndex] = sampleR;
    m_writeIndex = (m_writeIndex + 1) % m_capacity;
}

void AcousticDelayLine::read(float delaySeconds, float& outL, float& outR) const {
    if (m_capacity == 0) {
        outL = 0.0f;
        outR = 0.0f;
        return;
    }

    float delaySamples = delaySeconds * static_cast<float>(m_sampleRate);
    delaySamples = std::clamp(delaySamples, 0.0f, static_cast<float>(m_capacity - 2));

    float readPos = static_cast<float>(m_writeIndex) - delaySamples;
    while (readPos < 0.0f) readPos += static_cast<float>(m_capacity);

    size_t index0 = static_cast<size_t>(readPos) % m_capacity;
    size_t index1 = (index0 + 1) % m_capacity;
    float frac = readPos - std::floor(readPos);

    outL = m_bufferL[index0] * (1.0f - frac) + m_bufferL[index1] * frac;
    outR = m_bufferR[index0] * (1.0f - frac) + m_bufferR[index1] * frac;
}

void AcousticDelayLine::clear() {
    std::fill(m_bufferL.begin(), m_bufferL.end(), 0.0f);
    std::fill(m_bufferR.begin(), m_bufferR.end(), 0.0f);
    m_writeIndex = 0;
}

// =============================================================================
// SDL Audio Device Callback
// =============================================================================
static void sdlAudioCallbackProxy(void* userdata, Uint8* stream, int len) {
    auto* engine = static_cast<AlpineAudioEngine*>(userdata);
    float* floatStream = reinterpret_cast<float*>(stream);
    int numFrames = len / (2 * sizeof(float)); // stereo float samples
    engine->fillAudioBuffer(floatStream, numFrames);
}

// =============================================================================
// AlpineAudioEngine Implementation
// =============================================================================
AlpineAudioEngine::AlpineAudioEngine()
    : m_echoDelayLine(8.0f, 48000) {
    m_footstepVoices.resize(MAX_FOOTSTEP_VOICES);
}

AlpineAudioEngine::~AlpineAudioEngine() {
    shutdown();
}

bool AlpineAudioEngine::init(const game::TerrainCollider* collider) {
    if (collider) {
        m_raytracer = std::make_unique<AcousticRaytracer>(*collider);
    }

    // Initialize SDL Audio Subsystem
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "[Audio] Warning: Failed to init SDL Audio subsystem: " << SDL_GetError() << std::endl;
    }

    SDL_AudioSpec desiredSpec{};
    desiredSpec.freq = m_sampleRate;
    desiredSpec.format = AUDIO_F32SYS;
    desiredSpec.channels = 2;
    desiredSpec.samples = 1024;
    desiredSpec.callback = sdlAudioCallbackProxy;
    desiredSpec.userdata = this;

    SDL_AudioSpec obtainedSpec{};
    m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &obtainedSpec, 0);

    if (m_audioDevice == 0) {
        std::cerr << "[Audio] Note: No physical audio output opened (" << SDL_GetError()
                  << "). Offline WAV rendering and simulation remain fully active." << std::endl;
    } else {
        m_sampleRate = obtainedSpec.freq;
        SDL_PauseAudioDevice(m_audioDevice, 0); // Start playback
        std::cout << "[Audio] Wave-Based Alpine Audio Raytracing initialized ("
                  << m_sampleRate << " Hz stereo, buffer: " << obtainedSpec.samples << " samples)." << std::endl;
    }

    // Initialize DSP Filters
    m_windRumbleFilter.setLowpass(static_cast<float>(m_sampleRate), 95.0f, 0.65f);
    m_windVortexFilter.setBandpass(static_cast<float>(m_sampleRate), 450.0f, 3.8f);
    m_windSpindriftFilter.setBandpass(static_cast<float>(m_sampleRate), 3200.0f, 1.8f);

    m_nuptseEchoFilterL.setLowpass(static_cast<float>(m_sampleRate), 2600.0f, 0.7071f);
    m_nuptseEchoFilterR.setLowpass(static_cast<float>(m_sampleRate), 2600.0f, 0.7071f);

    m_initialized = true;
    return true;
}

void AlpineAudioEngine::shutdown() {
    if (m_audioDevice != 0) {
        SDL_CloseAudioDevice(m_audioDevice);
        m_audioDevice = 0;
    }
    m_initialized = false;
}

float AlpineAudioEngine::getWhiteNoise() {
    m_noiseSeed = m_noiseSeed * 1664525u + 1013904223u;
    return (static_cast<float>(m_noiseSeed >> 9) * (1.0f / 4194304.0f)) - 1.0f;
}

float AlpineAudioEngine::getPinkNoise() {
    float white = getWhiteNoise();
    m_pinkB0 = 0.99886f * m_pinkB0 + white * 0.0555179f;
    m_pinkB1 = 0.99332f * m_pinkB1 + white * 0.0750759f;
    m_pinkB2 = 0.96900f * m_pinkB2 + white * 0.1538520f;
    m_pinkB3 = 0.86650f * m_pinkB3 + white * 0.3104856f;
    m_pinkB4 = 0.55000f * m_pinkB4 + white * 0.5329522f;
    m_pinkB5 = -0.7616f * m_pinkB5 - white * 0.0168980f;
    float pink = m_pinkB0 + m_pinkB1 + m_pinkB2 + m_pinkB3 + m_pinkB4 + m_pinkB5 + m_pinkB6 + white * 0.5362f;
    m_pinkB6 = white * 0.115926f;
    return pink * 0.12f;
}

void AlpineAudioEngine::update(
    float deltaTime,
    const core::Camera& camera,
    const game::Player& player,
    const game::WeatherSystem& weather
) {
    std::lock_guard<std::mutex> lock(m_audioMutex);

    m_listenerPos = camera.getPosition();
    m_listenerForward = camera.getForward();
    m_listenerRight = camera.getRight();

    m_windSpeedKmh = weather.getWindSpeed();
    m_blizzardFactor = weather.getBlizzardFactor();
    m_windDirectionDeg = weather.getSummitWeather().windDirectionDeg;
    m_altitude = m_listenerPos.y;

    m_heartRateBpm = player.getHeartRateBpm();
    m_heartbeatPulse = player.getHeartbeatPulse();
    m_hypoxiaFactor = player.getHypoxiaFactor();
    m_exertion = player.getExertion();
    m_breathPhase = player.getBreathPhase();
    m_gogglesEquipped = player.isGogglesEquipped();

    // Periodic Acoustic Raytracing across the 35km DEM (every 0.25 seconds)
    m_raytraceTimer += deltaTime;
    if (m_raytracer && m_raytraceTimer >= 0.25f) {
        m_raytraceTimer = 0.0f;
        m_currentEnvironment = m_raytracer->traceEnvironment(m_listenerPos, m_listenerForward, m_listenerRight);
    }

    // Periodic natural icefall fracture (crevasses cracking in Khumbu Icefall & Western Cwm)
    m_ambientCrackTimer -= deltaTime;
    if (m_ambientCrackTimer <= 0.0f) {
        m_ambientCrackTimer = 6.0f + (getWhiteNoise() + 1.0f) * 4.0f;
        if (m_altitude > 5200.0f && m_altitude < 7200.0f) {
            // Natural sérac calving event in Khumbu Icefall
            glm::vec3 seracPos(-13200.0f, 5800.0f, -8200.0f);
            triggerIcefallCollapse(seracPos, 0.75f);
        }
    }
}

void AlpineAudioEngine::triggerFootstep(game::AlpineSurfaceType surface, float intensity, bool isLeftFoot) {
    std::lock_guard<std::mutex> lock(m_audioMutex);

    // Find free voice in pool
    FootstepVoice* voice = nullptr;
    for (auto& v : m_footstepVoices) {
        if (!v.active) {
            voice = &v;
            break;
        }
    }
    if (!voice) voice = &m_footstepVoices[0]; // steal oldest

    voice->active = true;
    voice->surfaceType = surface;
    voice->intensity = std::clamp(intensity, 0.2f, 2.0f);
    voice->timeSec = 0.0f;
    voice->durationSec = (surface == game::AlpineSurfaceType::TalusScreeSlope) ? 0.26f : 0.18f;
    voice->pan = isLeftFoot ? -0.22f : +0.22f;

    float sRate = static_cast<float>(m_sampleRate);
    if (surface == game::AlpineSurfaceType::GlacialBlueIce) {
        voice->filterL.setBandpass(sRate, 2600.0f, 3.5f);
        voice->filterR.setBandpass(sRate, 2600.0f, 3.5f);
    } else if (surface == game::AlpineSurfaceType::HardFirnSnow) {
        voice->filterL.setLowpass(sRate, 920.0f, 1.2f);
        voice->filterR.setLowpass(sRate, 920.0f, 1.2f);
    } else if (surface == game::AlpineSurfaceType::TalusScreeSlope) {
        voice->filterL.setBandpass(sRate, 1200.0f, 1.8f);
        voice->filterR.setBandpass(sRate, 1200.0f, 1.8f);
    } else { // ExposedRockFace
        voice->filterL.setHighpass(sRate, 1100.0f, 1.4f);
        voice->filterR.setHighpass(sRate, 1100.0f, 1.4f);
    }
}

void AlpineAudioEngine::triggerIcefallCollapse(const glm::vec3& worldPos, float magnitude) {
    std::lock_guard<std::mutex> lock(m_audioMutex);

    IceCollapseEvent ev{};
    ev.active = true;
    ev.worldPos = worldPos;
    ev.magnitude = std::clamp(magnitude, 0.2f, 2.5f);
    ev.timeSec = 0.0f;
    ev.durationSec = 7.5f;

    if (m_raytracer) {
        auto taps = m_raytracer->traceSourceToListener(worldPos, m_listenerPos, m_listenerRight);
        if (!taps.empty()) {
            ev.directDelaySec = taps[0].delaySeconds;
            ev.pan = taps[0].stereoPan;
            if (taps.size() > 1) {
                ev.nuptseEchoDelaySec = taps[1].delaySeconds;
            }
        }
    }

    m_iceCollapses.push_back(ev);
    std::cout << "[Audio] Infrasound Sérac Fracture triggered! Direct delay: "
              << std::fixed << std::setprecision(2) << ev.directDelaySec << "s | Nuptse Wall Echo delay: "
              << ev.nuptseEchoDelaySec << "s" << std::endl;
}

void AlpineAudioEngine::triggerAvalanche(float intensity) {
    // Avalanche center on Lhotse Face / Western Cwm
    glm::vec3 avalancheOrigin(-10800.0f, 7400.0f, -6800.0f);
    triggerIcefallCollapse(avalancheOrigin, intensity * 1.8f);
}

void AlpineAudioEngine::triggerWhumpf(const glm::vec3& worldPos, float intensity) {
    std::lock_guard<std::mutex> lock(m_audioMutex);

    m_whumpf.active = true;
    m_whumpf.timeSec = 0.0f;
    m_whumpf.durationSec = 0.38f;
    m_whumpf.frequency = 34.0f;
    m_whumpf.intensity = std::clamp(intensity, 0.4f, 2.5f);

    glm::vec3 toSource = worldPos - m_listenerPos;
    if (glm::length(toSource) > 0.1f) {
        m_whumpf.pan = std::clamp(glm::dot(glm::normalize(toSource), m_listenerRight), -0.8f, 0.8f);
    } else {
        m_whumpf.pan = 0.0f;
    }

    std::cout << "[Audio] Subterranean 'Whumpf' weak-layer collapse triggered! Intensity: "
              << m_whumpf.intensity << std::endl;
}

void AlpineAudioEngine::triggerCrownSnap(const glm::vec3& worldPos, float crackLength) {
    std::lock_guard<std::mutex> lock(m_audioMutex);

    m_crownSnap.active = true;
    m_crownSnap.timeSec = 0.0f;
    m_crownSnap.durationSec = 0.70f;
    m_crownSnap.crackLength = crackLength;
    m_crownSnap.intensity = 1.35f;

    glm::vec3 toSource = worldPos - m_listenerPos;
    float basePan = 0.0f;
    if (glm::length(toSource) > 0.1f) {
        basePan = std::clamp(glm::dot(glm::normalize(toSource), m_listenerRight), -0.8f, 0.8f);
    }
    m_crownSnap.panStart = std::clamp(basePan - 0.45f, -1.0f, 1.0f);
    m_crownSnap.panEnd = std::clamp(basePan + 0.45f, -1.0f, 1.0f);

    std::cout << "[Audio] Crown fracture tensile snap triggered! Crack breadth: "
              << crackLength << "m" << std::endl;
}

void AlpineAudioEngine::generateAudioFrame(float& left, float& right, float dt) {
    left = 0.0f;
    right = 0.0f;

    // =========================================================================
    // 1. Aeolian Mountain Wind & Ridge Karman Vortex Shedding
    // =========================================================================
    float windSpeed = m_windSpeedKmh;
    float windNorm = std::clamp(windSpeed / 120.0f, 0.05f, 2.5f);

    // Deep atmospheric turbulence (pink noise through lowpass)
    float pink = getPinkNoise();
    float rumble = m_windRumbleFilter.process(pink) * windNorm * 0.42f;

    // Resonant Karman Vortex Howl on sharp ridges (Hillary Step, Summit Ridge, Nuptse)
    float vortexFreq = 240.0f + (windSpeed / 100.0f) * 650.0f;
    float vortexQ = 3.2f + m_currentEnvironment.ridgeWindExposure * 3.5f;
    m_windVortexFilter.setBandpass(static_cast<float>(m_sampleRate), vortexFreq, vortexQ);

    float white = getWhiteNoise();
    float vortexHowl = m_windVortexFilter.process(white) * (0.15f + m_currentEnvironment.ridgeWindExposure * 0.65f) * windNorm;

    // Spindrift blowing ice crystals (high-frequency hiss)
    float spindriftHiss = m_windSpindriftFilter.process(white) * (0.05f + m_blizzardFactor * 0.55f) * windNorm;

    // Directional wind panning relative to listener head orientation
    float windRad = glm::radians(m_windDirectionDeg);
    glm::vec3 windVector(std::sin(windRad), 0.0f, std::cos(windRad));
    float windPan = std::clamp(glm::dot(windVector, m_listenerRight), -1.0f, 1.0f);

    float windGainL = std::clamp(1.0f - windPan * 0.4f, 0.2f, 1.2f);
    float windGainR = std::clamp(1.0f + windPan * 0.4f, 0.2f, 1.2f);

    float totalWind = rumble + vortexHowl + spindriftHiss;
    left += totalWind * windGainL;
    right += totalWind * windGainR;

    // =========================================================================
    // 2. Material-Specific Crampon Footstep Acoustics
    // =========================================================================
    for (auto& v : m_footstepVoices) {
        if (!v.active) continue;

        v.timeSec += dt;
        if (v.timeSec >= v.durationSec) {
            v.active = false;
            continue;
        }

        float t = v.timeSec;
        float env = std::pow(std::max(0.0f, 1.0f - (t / v.durationSec)), 2.5f);
        float stepSample = 0.0f;

        if (v.surfaceType == game::AlpineSurfaceType::GlacialBlueIce) {
            // Sharp steel front-point strikes (1850 Hz & 3450 Hz) + ice shard crackle
            float steelRing1 = std::sin(6.2831853f * 1850.0f * t) * std::exp(-85.0f * t);
            float steelRing2 = std::sin(6.2831853f * 3450.0f * t) * std::exp(-110.0f * t);
            float iceShatter = getWhiteNoise() * std::exp(-60.0f * t);
            stepSample = (steelRing1 * 0.6f + steelRing2 * 0.4f + iceShatter * 0.7f) * v.intensity;
        } else if (v.surfaceType == game::AlpineSurfaceType::HardFirnSnow) {
            // Squeaking compacted firn crunch (chirp from 580Hz down to 360Hz)
            float chirpFreq = 580.0f - (t / v.durationSec) * 220.0f;
            float snowSqueak = std::sin(6.2831853f * chirpFreq * t) * 0.35f;
            float snowCrunch = getPinkNoise() * 1.8f;
            stepSample = (snowSqueak + snowCrunch) * env * v.intensity;
        } else if (v.surfaceType == game::AlpineSurfaceType::TalusScreeSlope) {
            // Shifting stones and rattling gravel pebbles
            float stoneClick = std::sin(6.2831853f * 480.0f * t) * std::exp(-65.0f * t);
            float gravelRattle = getPinkNoise() * env;
            stepSample = (stoneClick * 0.8f + gravelRattle * 0.6f) * v.intensity;
        } else { // ExposedRockFace
            // Steel front-points grating harshly on sheer granite ledge
            float metalScrape = (getWhiteNoise() * 0.6f + std::sin(6.2831853f * 1400.0f * t) * 0.4f) * env;
            stepSample = metalScrape * v.intensity;
        }

        float fL = v.filterL.process(stepSample) * std::clamp(1.0f - v.pan, 0.1f, 1.0f);
        float fR = v.filterR.process(stepSample) * std::clamp(1.0f + v.pan, 0.1f, 1.0f);
        left += fL * 0.65f;
        right += fR * 0.65f;
    }

    // =========================================================================
    // 3. Infrasound Sérac Calving & Raytraced Nuptse Wall Echo
    // =========================================================================
    float echoSendL = 0.0f;
    float echoSendR = 0.0f;

    for (size_t i = 0; i < m_iceCollapses.size();) {
        auto& ev = m_iceCollapses[i];
        if (!ev.active) {
            i++;
            continue;
        }

        ev.timeSec += dt;
        if (ev.timeSec >= ev.durationSec + ev.nuptseEchoDelaySec + 3.0f) {
            ev.active = false;
            m_iceCollapses.erase(m_iceCollapses.begin() + i);
            continue;
        }

        float tDirect = ev.timeSec - ev.directDelaySec;
        if (tDirect > 0.0f && tDirect < ev.durationSec) {
            // Infrasound sub-bass sweep (42Hz down to 18Hz)
            float infraFreq = 42.0f - std::clamp(tDirect / 2.0f, 0.0f, 1.0f) * 24.0f;
            float infraSine = std::sin(6.2831853f * infraFreq * tDirect) * std::exp(-1.4f * tDirect);

            // Explosive fracture transient (initial 40ms snap)
            float snap = (tDirect < 0.045f) ? (getWhiteNoise() * (1.0f - tDirect / 0.045f) * 1.8f) : 0.0f;

            // Rolling avalanche rumble
            float avalancheRumble = getPinkNoise() * std::clamp(1.0f - tDirect / ev.durationSec, 0.0f, 1.0f) * 1.5f;

            float collapseSignal = (infraSine * 0.85f + snap * 0.95f + avalancheRumble * 0.6f) * ev.magnitude;

            float gL = collapseSignal * std::clamp(1.0f - ev.pan * 0.6f, 0.1f, 1.2f);
            float gR = collapseSignal * std::clamp(1.0f + ev.pan * 0.6f, 0.1f, 1.2f);

            left += gL * 0.7f;
            right += gR * 0.7f;

            echoSendL += gL;
            echoSendR += gR;
        }
        i++;
    }

    // =========================================================================
    // 3b. Step 24: Subterranean Weak-Layer "Whumpf" Collapse
    // =========================================================================
    if (m_whumpf.active) {
        m_whumpf.timeSec += dt;
        if (m_whumpf.timeSec >= m_whumpf.durationSec) {
            m_whumpf.active = false;
        } else {
            float t = m_whumpf.timeSec;
            // Sub-bass frequency sweep (drops from 40Hz down to 24Hz)
            float f = m_whumpf.frequency * (1.0f - 0.40f * (t / m_whumpf.durationSec));
            float sineSub = std::sin(6.2831853f * f * t);
            // Sharp initial shockwave envelope + heavy sub-bass decay
            float env = std::exp(-14.0f * t) + 0.35f * std::exp(-5.5f * t);
            float whumpfSample = (sineSub * 1.8f + getPinkNoise() * 0.45f) * env * m_whumpf.intensity;

            float panL = std::clamp(1.0f - m_whumpf.pan * 0.5f, 0.2f, 1.2f);
            float panR = std::clamp(1.0f + m_whumpf.pan * 0.5f, 0.2f, 1.2f);

            left += whumpfSample * panL * 0.95f;
            right += whumpfSample * panR * 0.95f;

            echoSendL += whumpfSample * panL * 0.65f;
            echoSendR += whumpfSample * panR * 0.65f;
        }
    }

    // =========================================================================
    // 3c. Step 24: Crown Fracture Tensile Snap (Slab Anrisskante Tearing)
    // =========================================================================
    if (m_crownSnap.active) {
        m_crownSnap.timeSec += dt;
        if (m_crownSnap.timeSec >= m_crownSnap.durationSec) {
            m_crownSnap.active = false;
        } else {
            float t = m_crownSnap.timeSec;
            float progress = t / m_crownSnap.durationSec;
            // Crack propagates across the slope: stereo pan sweeps across the face
            float currentPan = glm::mix(m_crownSnap.panStart, m_crownSnap.panEnd, progress);

            // Explosive crystalline crack burst (1400 Hz - 2600 Hz resonance)
            float snapFreq = 2200.0f - progress * 800.0f;
            float snapOsc = std::sin(6.2831853f * snapFreq * t);
            float fractalCrack = getWhiteNoise() * (std::sin(t * 850.0f) > 0.3f ? 1.4f : 0.2f);
            float snapEnv = std::exp(-9.0f * t) * (1.0f + 0.4f * std::sin(progress * 25.0f));

            float snapSample = (snapOsc * 0.75f + fractalCrack * 0.85f) * snapEnv * m_crownSnap.intensity;

            float snapL = snapSample * std::clamp(1.0f - currentPan * 0.6f, 0.1f, 1.3f);
            float snapR = snapSample * std::clamp(1.0f + currentPan * 0.6f, 0.1f, 1.3f);

            left += snapL * 0.85f;
            right += snapR * 0.85f;

            echoSendL += snapL * 0.75f;
            echoSendR += snapR * 0.75f;
        }
    }

    // Feed collapsing sound into acoustic delay line
    m_echoDelayLine.write(echoSendL, echoSendR);

    // Read back Raytraced Specular Echoes off the Nuptse 3,000m Wall
    for (const auto& tap : m_currentEnvironment.echoTaps) {
        float tapL = 0.0f, tapR = 0.0f;
        m_echoDelayLine.read(tap.delaySeconds, tapL, tapR);

        if (std::abs(tapL) > 0.0001f || std::abs(tapR) > 0.0001f) {
            float filteredL = m_nuptseEchoFilterL.process(tapL);
            float filteredR = m_nuptseEchoFilterR.process(tapR);

            float pL = filteredL * tap.gain * std::clamp(1.0f - tap.stereoPan * 0.5f, 0.1f, 1.2f);
            float pR = filteredR * tap.gain * std::clamp(1.0f + tap.stereoPan * 0.5f, 0.1f, 1.2f);

            left += pL;
            right += pR;
        }
    }

    // =========================================================================
    // 4. Physiological Audio: Cardiovascular Heartbeat & Heavy Respiration
    // =========================================================================
    if (m_hypoxiaFactor > 0.05f || m_exertion > 0.25f) {
        // Heartbeat throb in ears (dual-chamber "lub-dub")
        float pulseEnv = m_heartbeatPulse;
        if (pulseEnv > 0.01f) {
            float heartSine = std::sin(6.2831853f * 42.0f * (m_breathPhase * 2.0f));
            float heartThud = heartSine * pulseEnv * (0.10f + m_hypoxiaFactor * 0.35f);
            left += heartThud;
            right += heartThud;
        }

        // Cochlear Hypoxia Tinnitus (3,850 Hz high-pitch ringing in Death Zone)
        if (m_hypoxiaFactor > 0.45f) {
            static float tinnitusPhase = 0.0f;
            tinnitusPhase += dt * 3850.0f * 6.2831853f;
            if (tinnitusPhase > 6.2831853f) tinnitusPhase -= 6.2831853f;
            float tinnitus = std::sin(tinnitusPhase) * (m_hypoxiaFactor - 0.45f) * 0.035f;
            left += tinnitus;
            right += tinnitus;
        }

        // Heavy alpine respiration through balaclava / oxygen mask
        float exhalation = std::max(0.0f, std::sin(m_breathPhase));
        float breathAir = getPinkNoise() * exhalation * m_exertion * 0.22f;
        left += breathAir;
        right += breathAir;
    }

    // Soft master limiter / soft-clipping
    left = std::tanh(left * 0.85f);
    right = std::tanh(right * 0.85f);
}

void AlpineAudioEngine::fillAudioBuffer(float* stream, int numFrames) {
    std::lock_guard<std::mutex> lock(m_audioMutex);
    float dt = 1.0f / static_cast<float>(m_sampleRate);

    for (int i = 0; i < numFrames; i++) {
        float l = 0.0f, r = 0.0f;
        generateAudioFrame(l, r, dt);
        stream[i * 2 + 0] = l;
        stream[i * 2 + 1] = r;
    }
}

std::string AlpineAudioEngine::getAudioTelemetry() const {
    std::stringstream ss;
    ss << "[Audio] Wind: " << static_cast<int>(m_windSpeedKmh) << " km/h ("
       << (m_currentEnvironment.ridgeWindExposure > 0.5f ? "Ridge Howl" : "Valley Basin")
       << ") | RT60: " << std::fixed << std::setprecision(1) << m_currentEnvironment.reverberationTimeT60 << "s"
       << " | Nuptse Taps: " << m_currentEnvironment.echoTaps.size();
    return ss.str();
}

bool AlpineAudioEngine::renderToWav(
    const std::string& filepath,
    float durationSeconds,
    const core::Camera& camera,
    const game::Player& player,
    const game::WeatherSystem& weather
) {
    std::cout << "[Audio] Rendering offline WAV proof to " << filepath
              << " (" << durationSeconds << "s @ 48kHz stereo)..." << std::endl;

    update(0.016f, camera, player, weather);

    uint32_t sRate = static_cast<uint32_t>(m_sampleRate);
    uint32_t totalSamples = static_cast<uint32_t>(durationSeconds * static_cast<float>(sRate));
    uint16_t numChannels = 2;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sRate * numChannels * (bitsPerSample / 8);
    uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    uint32_t dataChunkSize = totalSamples * blockAlign;
    uint32_t fileSizeMinus8 = 36 + dataChunkSize;

    std::ofstream wav(filepath, std::ios::binary);
    if (!wav.is_open()) {
        std::cerr << "[Audio] Failed to open " << filepath << " for writing WAV!" << std::endl;
        return false;
    }

    // RIFF header
    wav.write("RIFF", 4);
    wav.write(reinterpret_cast<const char*>(&fileSizeMinus8), 4);
    wav.write("WAVE", 4);

    // fmt sub-chunk
    wav.write("fmt ", 4);
    uint32_t subChunk1Size = 16;
    uint16_t audioFormat = 1; // PCM
    wav.write(reinterpret_cast<const char*>(&subChunk1Size), 4);
    wav.write(reinterpret_cast<const char*>(&audioFormat), 2);
    wav.write(reinterpret_cast<const char*>(&numChannels), 2);
    wav.write(reinterpret_cast<const char*>(&sRate), 4);
    wav.write(reinterpret_cast<const char*>(&byteRate), 4);
    wav.write(reinterpret_cast<const char*>(&blockAlign), 2);
    wav.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data sub-chunk
    wav.write("data", 4);
    wav.write(reinterpret_cast<const char*>(&dataChunkSize), 4);

    // Trigger initial sérac fracture near Nuptse Wall
    glm::vec3 icePos(-11000.0f, 6800.0f, -6500.0f);
    triggerIcefallCollapse(icePos, 1.4f);

    bool avalancheSlabActive = m_whumpf.active;
    if (avalancheSlabActive) {
        m_whumpf.timeSec = 0.0f;
    }

    float dt = 1.0f / static_cast<float>(sRate);
    for (uint32_t i = 0; i < totalSamples; i++) {
        if (i == static_cast<uint32_t>(0.12f * static_cast<float>(sRate)) && avalancheSlabActive) {
            triggerCrownSnap(m_listenerPos + glm::vec3(15.0f, 5.0f, 20.0f), 180.0f);
        }

        // Trigger all 4 material footsteps sequentially to verify distinct acoustics
        if (i == static_cast<uint32_t>(0.4f * static_cast<float>(sRate))) {
            triggerFootstep(game::AlpineSurfaceType::GlacialBlueIce, 1.2f, false);
        } else if (i == static_cast<uint32_t>(1.5f * static_cast<float>(sRate))) {
            triggerFootstep(game::AlpineSurfaceType::HardFirnSnow, 1.1f, true);
        } else if (i == static_cast<uint32_t>(2.6f * static_cast<float>(sRate))) {
            triggerFootstep(game::AlpineSurfaceType::TalusScreeSlope, 1.2f, false);
        } else if (i == static_cast<uint32_t>(3.7f * static_cast<float>(sRate))) {
            triggerFootstep(game::AlpineSurfaceType::ExposedRockFace, 1.3f, true);
        }

        float l = 0.0f, r = 0.0f;
        generateAudioFrame(l, r, dt);

        int16_t pcmL = static_cast<int16_t>(std::clamp(l, -1.0f, 1.0f) * 32767.0f);
        int16_t pcmR = static_cast<int16_t>(std::clamp(r, -1.0f, 1.0f) * 32767.0f);

        wav.write(reinterpret_cast<const char*>(&pcmL), 2);
        wav.write(reinterpret_cast<const char*>(&pcmR), 2);
    }

    wav.close();
    std::cout << "[Audio] Successfully saved " << filepath << " (" << (dataChunkSize / 1024) << " KB)." << std::endl;
    return true;
}

} // namespace whiteout::audio
