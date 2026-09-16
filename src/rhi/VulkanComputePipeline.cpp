#include "VulkanComputePipeline.hpp"
#include "VulkanContext.hpp"
#include "VulkanShader.hpp"
#include <stdexcept>
#include <iostream>

namespace whiteout::rhi {

VulkanComputePipeline::VulkanComputePipeline(
    const VulkanContext& context,
    const std::string& compSpvPath,
    VkDescriptorSetLayout descriptorSetLayout,
    uint32_t pushConstantSize
) : m_context(context) {
    VkDevice device = m_context.getDevice();

    VkShaderModule compShaderModule = VulkanShader::createShaderModule(device, compSpvPath);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = pushConstantSize;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = (descriptorSetLayout != VK_NULL_HANDLE) ? 1 : 0;
    pipelineLayoutInfo.pSetLayouts = (descriptorSetLayout != VK_NULL_HANDLE) ? &descriptorSetLayout : nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = (pushConstantSize > 0) ? 1 : 0;
    pipelineLayoutInfo.pPushConstantRanges = (pushConstantSize > 0) ? &pushConstantRange : nullptr;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, compShaderModule, nullptr);
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = compShaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = m_layout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, compShaderModule, nullptr);
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    vkDestroyShaderModule(device, compShaderModule, nullptr);
}

VulkanComputePipeline::~VulkanComputePipeline() {
    VkDevice device = m_context.getDevice();
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_layout, nullptr);
    }
}

} // namespace whiteout::rhi
