#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace whiteout::game {

enum class GeologicalFormation {
    GreaterHimalayanCrystalline, // < 7,000m (Gneiss, Granite basement)
    NorthColFormation,           // 7,000m - 8,200m (Pelitic Schists & Phyllites)
    TheYellowBand,               // 8,200m - 8,600m (Golden Dolomitic Marble)
    QomolangmaFormation          // > 8,600m (Summit Limestone & Summit Marble)
};

enum class AlpineSurfaceType {
    HardFirnSnow,
    GlacialBlueIce,
    TalusScreeSlope,
    ExposedRockFace
};

struct AlpineGeologyInfo {
    GeologicalFormation formation;
    std::string formationName;
    AlpineSurfaceType surfaceType;
    std::string surfaceTypeName;
    float strataElevation;
    float slopeDegrees;
    float couloirFlute;
    float ambientTempCelsius;
    float windChillCelsius;
    float jetStreamSpeedKmh;
};

class TerrainCollider {
public:
    TerrainCollider(float worldWidth = 34610.0f, float worldDepth = 34520.0f);

    bool loadDem(const std::string& demBinPath, uint32_t width = 1024, uint32_t height = 1024);
    void generateProcedural(uint32_t width = 256, uint32_t height = 256);

    [[nodiscard]] float getHeight(float worldX, float worldZ) const;
    [[nodiscard]] glm::vec3 getNormal(float worldX, float worldZ) const;
    [[nodiscard]] float getSlopeAngleDegrees(float worldX, float worldZ) const;

    [[nodiscard]] AlpineGeologyInfo getGeologyInfo(float worldX, float worldZ) const;

    [[nodiscard]] float getWorldWidth() const { return m_worldWidth; }
    [[nodiscard]] float getWorldDepth() const { return m_worldDepth; }
    [[nodiscard]] bool isLoaded() const { return !m_elevationData.empty(); }

private:
    float m_worldWidth = 34610.0f;
    float m_worldDepth = 34520.0f;
    uint32_t m_gridWidth = 1024;
    uint32_t m_gridHeight = 1024;
    std::vector<float> m_elevationData;
};

} // namespace whiteout::game
