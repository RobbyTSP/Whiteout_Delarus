#include "WeatherSystem.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <algorithm>

namespace whiteout::game {

namespace {

float parseJsonFloat(const std::string& content, const std::string& key, float defaultVal = 0.0f) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return defaultVal;

    size_t colon = content.find(':', pos);
    if (colon == std::string::npos) return defaultVal;

    size_t start = content.find_first_not_of(" \t\r\n", colon + 1);
    if (start == std::string::npos) return defaultVal;

    try {
        return std::stof(content.substr(start));
    } catch (...) {
        return defaultVal;
    }
}

} // namespace

WeatherSystem::WeatherSystem(const std::string& weatherJsonPath)
    : m_jsonPath(weatherJsonPath) {
    loadWeatherJson(m_jsonPath);
    setTimeOfDayPreset(TimeOfDayPreset::DawnAlpengluehen);
}

bool WeatherSystem::loadWeatherJson(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "[WeatherSystem] Could not open " << jsonPath << ", using default Himalayan climate." << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonStr = buffer.str();
    file.close();

    // 1. Everest Summit section
    size_t summitPos = jsonStr.find("\"everest_summit\"");
    if (summitPos != std::string::npos) {
        std::string summitSub = jsonStr.substr(summitPos, 600);
        m_summitWeather.stationName = "Mount Everest Summit (8,848m)";
        m_summitWeather.temperatureCelsius = parseJsonFloat(summitSub, "temperature_celsius", -20.7f);
        m_summitWeather.apparentTempCelsius = parseJsonFloat(summitSub, "apparent_temperature_celsius", -24.7f);
        m_summitWeather.windSpeedKmh = parseJsonFloat(summitSub, "wind_speed_kmh", 25.0f);
        m_summitWeather.windGustsKmh = parseJsonFloat(summitSub, "wind_gusts_kmh", 45.0f);
        m_summitWeather.windDirectionDeg = parseJsonFloat(summitSub, "wind_direction_deg", 135.0f);
        m_summitWeather.cloudCoverPercent = parseJsonFloat(summitSub, "cloud_cover_percent", 75.0f);
        m_summitWeather.surfacePressureHpa = parseJsonFloat(summitSub, "surface_pressure_hpa", 354.5f);
    }

    // 2. Base Camp section
    size_t bcPos = jsonStr.find("\"everest_base_camp\"");
    if (bcPos != std::string::npos) {
        std::string bcSub = jsonStr.substr(bcPos, 600);
        m_baseCampWeather.stationName = "Everest Base Camp (5,364m)";
        m_baseCampWeather.temperatureCelsius = parseJsonFloat(bcSub, "temperature_celsius", 0.4f);
        m_baseCampWeather.apparentTempCelsius = parseJsonFloat(bcSub, "apparent_temperature_celsius", -2.2f);
        m_baseCampWeather.windSpeedKmh = parseJsonFloat(bcSub, "wind_speed_kmh", 12.0f);
        m_baseCampWeather.windGustsKmh = parseJsonFloat(bcSub, "wind_gusts_kmh", 25.0f);
        m_baseCampWeather.windDirectionDeg = parseJsonFloat(bcSub, "wind_direction_deg", 35.0f);
        m_baseCampWeather.cloudCoverPercent = parseJsonFloat(bcSub, "cloud_cover_percent", 73.0f);
        m_baseCampWeather.surfacePressureHpa = parseJsonFloat(bcSub, "surface_pressure_hpa", 552.5f);
    }

    m_cloudDensity = std::clamp(m_summitWeather.cloudCoverPercent / 100.0f, 0.20f, 0.95f);

    std::cout << "[WeatherSystem] Synced Live Open-Meteo Weather:\n"
              << "  Summit: " << m_summitWeather.temperatureCelsius << "°C | Wind: "
              << m_summitWeather.windSpeedKmh << " km/h (Gusts " << m_summitWeather.windGustsKmh << " km/h) | Clouds: "
              << m_summitWeather.cloudCoverPercent << "%\n"
              << "  Base Camp: " << m_baseCampWeather.temperatureCelsius << "°C | Wind: "
              << m_baseCampWeather.windSpeedKmh << " km/h | Pressure: "
              << m_baseCampWeather.surfacePressureHpa << " hPa" << std::endl;

    return true;
}

void WeatherSystem::setTimeOfDayPreset(TimeOfDayPreset preset) {
    m_timePreset = preset;

    switch (preset) {
        case TimeOfDayPreset::DawnAlpengluehen:
            m_hourOfDay = 5.85f; // 05:51 AM
            // Sun low on eastern horizon (~6° elevation, 82° azimuth)
            calculateSolarPosition(m_hourOfDay);
            // Blazing golden-rose / fiery pink Alpenglühen hitting summit pyramid
            m_sunColor = glm::vec3(1.78f, 1.05f, 0.62f);
            m_cloudBase = 4400.0f;
            m_cloudDensity = 0.85f;
            std::cout << "[WeatherSystem] Time: Dawn Alpenglühen (05:51) - First golden-rose light on Everest Summit" << std::endl;
            break;

        case TimeOfDayPreset::CrispNoon:
            m_hourOfDay = 12.0f; // 12:00 PM
            // Sun high overhead (~72° elevation)
            calculateSolarPosition(m_hourOfDay);
            // Brilliant stratospheric midday sun
            m_sunColor = glm::vec3(1.30f, 1.25f, 1.15f);
            m_cloudBase = 4400.0f;
            m_cloudDensity = 0.0f; // Crystal-clear alpine visibility at noon!
            std::cout << "[WeatherSystem] Time: Crisp Midday (12:00) - Harsh stratospheric sun & sharp shadows" << std::endl;
            break;

        case TimeOfDayPreset::SunsetAlpengluehen:
            m_hourOfDay = 18.35f; // 18:21 PM
            // Sun low on western horizon (~5° elevation, 278° azimuth)
            calculateSolarPosition(m_hourOfDay);
            // Deep crimson, ruby-amber evening Alpenglühen
            m_sunColor = glm::vec3(1.85f, 0.72f, 0.38f);
            m_cloudBase = 4450.0f;
            m_cloudDensity = 0.80f;
            std::cout << "[WeatherSystem] Time: Sunset Alpenglühen (18:21) - Fiery crimson glow across Lhotse & Everest" << std::endl;
            break;

        case TimeOfDayPreset::MoonlitNight:
            m_hourOfDay = 22.5f; // 22:30 PM
            // Moon high in southern sky
            m_sunDir = glm::normalize(glm::vec3(-0.35f, 0.75f, 0.55f));
            // Cool silvery-blue lunar illumination on snow & glaciers
            m_sunColor = glm::vec3(0.14f, 0.18f, 0.32f);
            m_cloudBase = 4300.0f;
            m_cloudDensity = 0.40f;
            std::cout << "[WeatherSystem] Time: Moonlit Night (22:30) - Silver moonlight over frozen Himalayan glaciers" << std::endl;
            break;

        case TimeOfDayPreset::LiveRealtime:
            // Calculate approximate current sun position
            m_hourOfDay = 10.5f;
            calculateSolarPosition(m_hourOfDay);
            m_sunColor = glm::vec3(1.35f, 1.28f, 1.18f);
            std::cout << "[WeatherSystem] Time: Live Realtime Solar Tracking" << std::endl;
            break;
    }
}

void WeatherSystem::cycleTimeOfDay() {
    int next = (static_cast<int>(m_timePreset) + 1) % 4; // Cycle through 4 iconic presets
    setTimeOfDayPreset(static_cast<TimeOfDayPreset>(next));
}

void WeatherSystem::toggleBlizzard() {
    m_blizzardActive = !m_blizzardActive;
    m_blizzardFactor = m_blizzardActive ? 1.0f : 0.0f;
    if (m_blizzardActive) {
        std::cout << "\n[WeatherSystem] WARNING: Blizzard & Whiteout condition initiated!\n"
                  << "  Gale-force winds: 145 km/h\n"
                  << "  Visibility collapsing to ~30 meters\n"
                  << "  Heavy spindrift and ice crystal storm\n" << std::endl;
    } else {
        std::cout << "[WeatherSystem] Blizzard cleared. Atmospheric visibility restored to 150 km." << std::endl;
    }
}

void WeatherSystem::toggleLiveWeather() {
    m_useLiveWeather = !m_useLiveWeather;
    if (m_useLiveWeather) {
        loadWeatherJson(m_jsonPath);
        std::cout << "[WeatherSystem] Switched to LIVE Open-Meteo Himalayan Weather Sync." << std::endl;
    } else {
        std::cout << "[WeatherSystem] Switched to Manual Weather Simulation Mode." << std::endl;
    }
}

void WeatherSystem::calculateSolarPosition(float hour24) {
    // Solar trajectory across Himalayan latitude (~28° N)
    // 6:00 is East, 12:00 is South, 18:00 is West
    float angle = (hour24 - 6.0f) / 12.0f * 3.14159265f;
    float elev = std::sin(angle);
    float azim = std::cos(angle);

    float sx = -azim;
    float sy = std::max(0.06f, elev);
    float sz = std::cos(elev * 1.57079f) * 0.45f;

    m_sunDir = glm::normalize(glm::vec3(sx, sy, sz));
}

void WeatherSystem::update(float deltaTime) {
    // Smooth transition into/out of blizzard whiteout conditions
    float targetBlizzard = m_blizzardActive ? 1.0f : 0.0f;
    m_blizzardFactor = glm::mix(m_blizzardFactor, targetBlizzard, std::clamp(3.0f * deltaTime, 0.0f, 1.0f));

    // Dynamic sea of clouds base undulation
    m_cloudBase = 4900.0f + std::sin(m_hourOfDay * 0.5f) * 80.0f;
}

glm::vec3 WeatherSystem::getSunDirection() const {
    return m_sunDir;
}

glm::vec3 WeatherSystem::getSunColor() const {
    // In blizzard whiteout: direct sun is heavily obscured by storm clouds
    float stormDim = 1.0f - m_blizzardFactor * 0.70f;
    return m_sunColor * stormDim;
}

float WeatherSystem::getCloudDensity() const {
    if (m_blizzardFactor > 0.01f) {
        return glm::mix(m_cloudDensity, 1.0f, m_blizzardFactor);
    }
    return m_cloudDensity;
}

float WeatherSystem::getWindSpeed() const {
    float baseWind = m_useLiveWeather ? m_summitWeather.windSpeedKmh : 30.0f;
    if (m_blizzardActive) {
        return std::max(baseWind, 145.0f);
    }
    return baseWind;
}

std::string WeatherSystem::getWeatherTelemetry() const {
    std::stringstream ss;
    ss << "Weather: ";
    if (m_blizzardActive) {
        ss << "[WHITEOUT BLIZZARD - Gale " << static_cast<int>(getWindSpeed()) << " km/h] | Vis: 30m";
    } else {
        std::string timeName;
        switch (m_timePreset) {
            case TimeOfDayPreset::DawnAlpengluehen: timeName = "Dawn Alpenglühen (05:51)"; break;
            case TimeOfDayPreset::CrispNoon:        timeName = "Crisp Midday (12:00)"; break;
            case TimeOfDayPreset::SunsetAlpengluehen: timeName = "Sunset Alpenglühen (18:21)"; break;
            case TimeOfDayPreset::MoonlitNight:     timeName = "Moonlit Night (22:30)"; break;
            default: timeName = "Live Clock"; break;
        }
        ss << timeName << " | Wolkenmeer: " << static_cast<int>(m_cloudDensity * 100.0f) << "% @ "
           << static_cast<int>(m_cloudBase) << "m | Summit: "
           << std::fixed << std::setprecision(1) << m_summitWeather.temperatureCelsius << "°C";
    }

    ss << " | [T]: Time | [B]: Blizzard | [L]: Live Weather";
    return ss.str();
}

} // namespace whiteout::game
