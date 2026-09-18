#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace whiteout::rhi {

class VulkanContext;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct TerrainPushConstants {
    glm::mat4 model;      // 64 bytes
    glm::mat4 view;       // 64 bytes
    glm::mat4 proj;       // 64 bytes
    glm::vec4 cameraPos;  // 16 bytes: xyz = camera world position, w = cloudDensity (0..1)
    glm::vec4 sunDir;     // 16 bytes: xyz = sun direction, w = windSpeed (km/h)
    glm::vec4 sunColor;   // 16 bytes: rgb = sun irradiance spectrum, w = blizzardFactor (0..1)
    float time;           // 4 bytes: elapsed time
    float minElev;        // 4 bytes: minimum elevation (3651m)
    float maxElev;        // 4 bytes: maximum elevation (8753m)
    float cloudBase;      // 4 bytes: Wolkenmeer sea of clouds base elevation (~4900m)
};

struct PostProcessPushConstants {
    glm::vec4 sunScreenPos; // xy = screen UV [0, 1], z = isVisible (0/1), w = sunIntensity
    glm::vec2 resolution;   // screen width, height
    float time;             // total elapsed time
    float blizzard;         // blizzard factor [0, 1]
    glm::vec4 cryoParams1;  // x = gogglesEquipped (0/1), y = gogglesFog (0..1), z = frostAmount (0..1), w = snowBlindness (0..1)
    glm::vec4 cryoParams2;  // x = hypoxiaFactor (0..1), y = heartbeatPulse (0..1), z = oxygenSaturation (0..100), w = altitude (m)
};

class VulkanPipeline {
public:
    VulkanPipeline(
        const VulkanContext& context,
        VkFormat colorFormat,
        VkFormat depthFormat,
        const std::string& vertSpvPath,
        const std::string& fragSpvPath,
        bool isSky = false
    );

    // Constructor for custom / post-process graphics pipelines
    VulkanPipeline(
        const VulkanContext& context,
        VkFormat colorFormat,
        VkFormat depthFormat,
        const std::string& vertSpvPath,
        const std::string& fragSpvPath,
        VkDescriptorSetLayout externalDescriptorLayout,
        uint32_t pushConstantSize,
        bool isFullscreen = true
    );

    // Constructor for arbitrary vertex & instance binding descriptions
    VulkanPipeline(
        const VulkanContext& context,
        VkFormat colorFormat,
        VkFormat depthFormat,
        const std::string& vertSpvPath,
        const std::string& fragSpvPath,
        const std::vector<VkVertexInputBindingDescription>& bindingDescriptions,
        const std::vector<VkVertexInputAttributeDescription>& attributeDescriptions,
        VkDescriptorSetLayout externalDescriptorLayout = VK_NULL_HANDLE,
        uint32_t pushConstantSize = sizeof(TerrainPushConstants),
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
        bool enableAlphaBlend = false,
        bool depthWrite = true
    );

    ~VulkanPipeline();

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    [[nodiscard]] VkPipeline getHandle() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout getLayout() const { return m_layout; }
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }

private:
    const VulkanContext& m_context;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    bool m_ownsDescriptorSetLayout = false;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

} // namespace whiteout::rhi
