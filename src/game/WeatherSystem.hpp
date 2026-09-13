#pragma once

#include <glm/glm.hpp>
#include <string>

namespace whiteout::game {

enum class TimeOfDayPreset {
    DawnAlpengluehen = 0, // 05:45 AM - Fiery rose-gold first light on Everest summit
    CrispNoon        = 1, // 12:00 PM - Harsh stratospheric midday sun
    SunsetAlpengluehen = 2, // 18:20 PM - Crimson and amber twilight glow
    MoonlitNight     = 3, // 22:30 PM - Diamond snow reflection under moonlight
    LiveRealtime     = 4  // Synced with real-time Nepal clock (UTC+5:45)
};

struct StationWeatherData {
    std::string stationName;
    float temperatureCelsius = -20.7f;
    float apparentTempCelsius = -24.7f;
    float windSpeedKmh = 25.0f;
    float windGustsKmh = 45.0f;
    float windDirectionDeg = 135.0f;
    float cloudCoverPercent = 75.0f;
    float surfacePressureHpa = 354.5f;
    int weatherCode = 3;
};

class WeatherSystem {
public:
    explicit WeatherSystem(const std::string& weatherJsonPath = "data/weather/everest_current.json");

    void update(float deltaTime);

    void cycleTimeOfDay();
    void setTimeOfDayPreset(TimeOfDayPreset preset);
    void toggleBlizzard();
    void toggleLiveWeather();

    bool loadWeatherJson(const std::string& jsonPath);

    [[nodiscard]] glm::vec3 getSunDirection() const;
    [[nodiscard]] glm::vec3 getSunColor() const;
    [[nodiscard]] float getCloudDensity() const;
    [[nodiscard]] float getCloudBase() const { return m_cloudBase; }
    [[nodiscard]] float getBlizzardFactor() const { return m_blizzardFactor; }
    [[nodiscard]] float getWindSpeed() const;
    [[nodiscard]] bool isBlizzardActive() const { return m_blizzardActive; }
    [[nodiscard]] bool isLiveWeatherActive() const { return m_useLiveWeather; }
    [[nodiscard]] TimeOfDayPreset getTimeOfDayPreset() const { return m_timePreset; }

    [[nodiscard]] const StationWeatherData& getSummitWeather() const { return m_summitWeather; }
    [[nodiscard]] const StationWeatherData& getBaseCampWeather() const { return m_baseCampWeather; }

    [[nodiscard]] std::string getWeatherTelemetry() const;

private:
    void calculateSolarPosition(float hour24);

    std::string m_jsonPath;
    StationWeatherData m_summitWeather;
    StationWeatherData m_baseCampWeather;

    TimeOfDayPreset m_timePreset = TimeOfDayPreset::DawnAlpengluehen;
    bool m_useLiveWeather = true;
    bool m_blizzardActive = false;
    float m_blizzardFactor = 0.0f; // 0.0 = calm, 1.0 = intense whiteout

    float m_hourOfDay = 6.25f; // 06:15 AM
    glm::vec3 m_sunDir{0.4f, 0.75f, 0.45f};
    glm::vec3 m_sunColor{1.65f, 1.15f, 0.75f};

    float m_cloudBase = 4950.0f; // Inversion cloud layer base in Khumbu valley
    float m_cloudDensity = 0.75f; // Sea of clouds density
};

} // namespace whiteout::game
