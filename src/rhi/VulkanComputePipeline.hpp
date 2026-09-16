#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace whiteout::rhi {

class VulkanContext;

class VulkanComputePipeline {
public:
    VulkanComputePipeline(
        const VulkanContext& context,
        const std::string& compSpvPath,
        VkDescriptorSetLayout descriptorSetLayout,
        uint32_t pushConstantSize = 0
    );
    ~VulkanComputePipeline();

    VulkanComputePipeline(const VulkanComputePipeline&) = delete;
    VulkanComputePipeline& operator=(const VulkanComputePipeline&) = delete;

    [[nodiscard]] VkPipeline getHandle() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout getLayout() const { return m_layout; }

private:
    const VulkanContext& m_context;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

} // namespace whiteout::rhi
