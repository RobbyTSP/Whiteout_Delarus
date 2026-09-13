#include "TerrainCollider.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace whiteout::game {

TerrainCollider::TerrainCollider(float worldWidth, float worldDepth)
    : m_worldWidth(worldWidth), m_worldDepth(worldDepth) {}

bool TerrainCollider::loadDem(const std::string& demBinPath, uint32_t width, uint32_t height) {
    m_gridWidth = width;
    m_gridHeight = height;

    std::ifstream file(demBinPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[TerrainCollider] Failed to open DEM: " << demBinPath << std::endl;
        generateProcedural(256, 256);
        return false;
    }

    m_elevationData.resize(m_gridWidth * m_gridHeight);
    file.read(reinterpret_cast<char*>(m_elevationData.data()), m_elevationData.size() * sizeof(float));
    file.close();

    std::cout << "[TerrainCollider] Loaded DEM collision matrix (" << m_gridWidth << "x" << m_gridHeight << ") for 1:1 Himalayas." << std::endl;
    return true;
}

void TerrainCollider::generateProcedural(uint32_t width, uint32_t height) {
    m_gridWidth = width;
    m_gridHeight = height;
    m_elevationData.resize(width * height);

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            float u = static_cast<float>(x) / static_cast<float>(width - 1);
            float v = static_cast<float>(y) / static_cast<float>(height - 1);
            float posX = (u - 0.5f) * m_worldWidth;
            float posZ = (v - 0.5f) * m_worldDepth;

            float dist = std::sqrt(posX * posX + posZ * posZ) / 18000.0f;
            float peak = std::exp(-dist * dist * 3.0f);
            float noise = std::sin(posX * 0.0005f) * std::cos(posZ * 0.0005f) * 600.0f;
            m_elevationData[y * width + x] = 4000.0f + peak * 4848.0f + noise;
        }
    }
}

float TerrainCollider::getHeight(float worldX, float worldZ) const {
    if (m_elevationData.empty()) return 0.0f;

    // Convert world space to normalized [0..1] UV coordinates
    float u = (worldX / m_worldWidth) + 0.5f;
    float v = (worldZ / m_worldDepth) + 0.5f;

    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float gx = u * static_cast<float>(m_gridWidth - 1);
    float gz = v * static_cast<float>(m_gridHeight - 1);

    uint32_t x0 = static_cast<uint32_t>(std::floor(gx));
    uint32_t z0 = static_cast<uint32_t>(std::floor(gz));
    uint32_t x1 = std::min(x0 + 1, m_gridWidth - 1);
    uint32_t z1 = std::min(z0 + 1, m_gridHeight - 1);

    float tx = gx - static_cast<float>(x0);
    float tz = gz - static_cast<float>(z0);

    float h00 = m_elevationData[z0 * m_gridWidth + x0];
    float h10 = m_elevationData[z0 * m_gridWidth + x1];
    float h01 = m_elevationData[z1 * m_gridWidth + x0];
    float h11 = m_elevationData[z1 * m_gridWidth + x1];

    // Bilinear interpolation
    float h0 = h00 * (1.0f - tx) + h10 * tx;
    float h1 = h01 * (1.0f - tx) + h11 * tx;

    return h0 * (1.0f - tz) + h1 * tz;
}

glm::vec3 TerrainCollider::getNormal(float worldX, float worldZ) const {
    constexpr float eps = 4.0f; // 4 meters delta
    float hL = getHeight(worldX - eps, worldZ);
    float hR = getHeight(worldX + eps, worldZ);
    float hD = getHeight(worldX, worldZ - eps);
    float hU = getHeight(worldX, worldZ + eps);

    float dx = (hR - hL) / (2.0f * eps);
    float dz = (hU - hD) / (2.0f * eps);

    return glm::normalize(glm::vec3(-dx, 1.0f, -dz));
}

float TerrainCollider::getSlopeAngleDegrees(float worldX, float worldZ) const {
    glm::vec3 n = getNormal(worldX, worldZ);
    float cosAngle = std::clamp(n.y, 0.0f, 1.0f);
    return glm::degrees(std::acos(cosAngle));
}

AlpineGeologyInfo TerrainCollider::getGeologyInfo(float worldX, float worldZ) const {
    AlpineGeologyInfo info{};
    float height = getHeight(worldX, worldZ);
    glm::vec3 normal = getNormal(worldX, worldZ);
    float slope = getSlopeAngleDegrees(worldX, worldZ);
    info.slopeDegrees = slope;

    // 1. Dipping strata height (15° northward dip of South Tibetan Detachment system)
    float strataH = height - 0.22f * worldZ;
    info.strataElevation = strataH;

    if (strataH >= 8600.0f) {
        info.formation = GeologicalFormation::QomolangmaFormation;
        info.formationName = "Qomolangma Summit Formation (Ordovician Limestone/Marble)";
    } else if (strataH >= 8180.0f) {
        info.formation = GeologicalFormation::TheYellowBand;
        info.formationName = "The Yellow Band (Golden Dolomitic Marble Strata)";
    } else if (strataH >= 7000.0f) {
        info.formation = GeologicalFormation::NorthColFormation;
        info.formationName = "North Col Formation (Pelitic Schists & Phyllites)";
    } else {
        info.formation = GeologicalFormation::GreaterHimalayanCrystalline;
        info.formationName = "Greater Himalayan Crystalline (Granite/Gneiss Basement)";
    }

    // 2. Directional couloir fluting wave
    glm::vec2 slopeFallLine = glm::normalize(glm::vec2(normal.x, normal.z) + glm::vec2(0.0001f));
    glm::vec2 perpAcrossSlope(-slopeFallLine.y, slopeFallLine.x);
    float fluteCoord = worldX * perpAcrossSlope.x + worldZ * perpAcrossSlope.y;
    float flute = std::sin(fluteCoord * 0.25f) * 0.55f + 
                  std::sin(fluteCoord * 0.78f) * 0.30f + 
                  std::sin(fluteCoord * 2.20f) * 0.15f;
    info.couloirFlute = flute;

    // 3. Alpine Surface classification
    if (slope > 42.0f) {
        if (flute < -0.15f) {
            info.surfaceType = AlpineSurfaceType::HardFirnSnow;
            info.surfaceTypeName = "Couloir Firn Snow Chute";
        } else {
            info.surfaceType = AlpineSurfaceType::ExposedRockFace;
            info.surfaceTypeName = "Exposed Rock Face (Granite/Limestone Cliff)";
        }
    } else if (slope >= 24.0f && slope <= 40.0f) {
        info.surfaceType = AlpineSurfaceType::TalusScreeSlope;
        info.surfaceTypeName = "Unstable Talus Scree Fan (Loose Gravel Repose)";
    } else if (height >= 4800.0f && height <= 5450.0f && slope < 22.0f) {
        info.surfaceType = AlpineSurfaceType::GlacialBlueIce;
        info.surfaceTypeName = "Glacial Blue Ice (Khumbu Glacier Icefall)";
    } else {
        info.surfaceType = AlpineSurfaceType::HardFirnSnow;
        info.surfaceTypeName = "Alpine Firn Snowfield";
    }

    // 4. Altitude-based climate and jet stream meteorology
    // Standard lapse rate: 6.5°C per 1000m from +15°C at sea level
    float baseTemp = 15.0f - (height / 1000.0f) * 6.5f;
    info.ambientTempCelsius = baseTemp;

    // Jet stream wind speed: scales from ~20 km/h at base valleys to 140+ km/h on Everest summit ridge
    float altRatio = std::clamp((height - 4000.0f) / 4848.0f, 0.0f, 1.0f);
    float windSpeed = 18.0f + 125.0f * (altRatio * altRatio);
    info.jetStreamSpeedKmh = windSpeed;

    // Windchill calculation (Osczevski & Bluestein index)
    float vP = std::pow(std::max(windSpeed, 5.0f), 0.16f);
    float chill = 13.12f + 0.6215f * baseTemp - 11.37f * vP + 0.3965f * baseTemp * vP;
    info.windChillCelsius = chill;

    return info;
}

} // namespace whiteout::game
