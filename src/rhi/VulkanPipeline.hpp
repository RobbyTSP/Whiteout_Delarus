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
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;
    glm::vec4 sunDir;
    glm::vec4 sunColor;
    float time;
    float minElev;
    float maxElev;
    float padding;
};

class VulkanPipeline {
public:
    VulkanPipeline(
        const VulkanContext& context,
        VkFormat colorFormat,
        VkFormat depthFormat,
        const std::string& vertSpvPath,
        const std::string& fragSpvPath
    );
    ~VulkanPipeline();

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    [[nodiscard]] VkPipeline getHandle() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout getLayout() const { return m_layout; }

private:
    const VulkanContext& m_context;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

} // namespace whiteout::rhi
