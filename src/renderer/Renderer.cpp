#include "Renderer.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include "../rhi/VulkanTexture.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

namespace whiteout::renderer {

Renderer::Renderer(core::Window& window)
    : m_window(window) {
    m_context = std::make_unique<rhi::VulkanContext>(m_window);
    m_swapchain = std::make_unique<rhi::VulkanSwapchain>(*m_context, m_window);

    // Step 17: Create 16-Bit Half-Float HDR Render Target (VK_FORMAT_R16G16B16A16_SFLOAT)
    createHdrResources();

    std::string vertSpv = SHADER_DIR "/terrain_vert.spv";
    std::string fragSpv = SHADER_DIR "/terrain_frag.spv";

    // Terrain Graphics Pipeline renders into HDR Color Attachment
    m_pipeline = std::make_unique<rhi::VulkanPipeline>(
        *m_context,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        m_swapchain->getDepthFormat(),
        vertSpv,
        fragSpv
    );

    std::string skyVertSpv = SHADER_DIR "/sky_vert.spv";
    std::string skyFragSpv = SHADER_DIR "/sky_frag.spv";

    // Sky Graphics Pipeline renders into HDR Color Attachment
    m_skyPipeline = std::make_unique<rhi::VulkanPipeline>(
        *m_context,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        m_swapchain->getDepthFormat(),
        skyVertSpv,
        skyFragSpv,
        true // isSky
    );

    // Step 18: CDLOD Micro-Terrain Pipeline (0.25m Climbing Resolution)
    std::string microVertSpv = SHADER_DIR "/micro_terrain_vert.spv";
    std::string microFragSpv = SHADER_DIR "/micro_terrain_frag.spv";
    auto microBinding = rhi::Vertex::getBindingDescription();
    auto microAttrs = rhi::Vertex::getAttributeDescriptions();
    m_microPipeline = std::make_unique<rhi::VulkanPipeline>(
        *m_context,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        m_swapchain->getDepthFormat(),
        microVertSpv,
        microFragSpv,
        std::vector<VkVertexInputBindingDescription>{microBinding},
        microAttrs,
        m_pipeline->getDescriptorSetLayout(),
        sizeof(rhi::TerrainPushConstants),
        VK_CULL_MODE_NONE
    );

    // Step 18: 3D Boulder & Talus Instancing Pipeline
    std::vector<VkVertexInputBindingDescription> boulderBindings(2);
    boulderBindings[0].binding = 0;
    boulderBindings[0].stride = sizeof(rhi::Vertex);
    boulderBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    boulderBindings[1].binding = 1;
    boulderBindings[1].stride = sizeof(BoulderInstanceData);
    boulderBindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::vector<VkVertexInputAttributeDescription> boulderAttributes;
    boulderAttributes.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(rhi::Vertex, position))});
    boulderAttributes.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(rhi::Vertex, normal))});
    boulderAttributes.push_back({2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(rhi::Vertex, uv))});

    boulderAttributes.push_back({3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(BoulderInstanceData, model) + 0 * sizeof(glm::vec4))});
    boulderAttributes.push_back({4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(BoulderInstanceData, model) + 1 * sizeof(glm::vec4))});
    boulderAttributes.push_back({5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(BoulderInstanceData, model) + 2 * sizeof(glm::vec4))});
    boulderAttributes.push_back({6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(BoulderInstanceData, model) + 3 * sizeof(glm::vec4))});
    boulderAttributes.push_back({7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(BoulderInstanceData, params))});

    std::string boulderVertSpv = SHADER_DIR "/boulder_vert.spv";
    std::string boulderFragSpv = SHADER_DIR "/boulder_frag.spv";
    m_boulderPipeline = std::make_unique<rhi::VulkanPipeline>(
        *m_context,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        m_swapchain->getDepthFormat(),
        boulderVertSpv,
        boulderFragSpv,
        boulderBindings,
        boulderAttributes,
        m_pipeline->getDescriptorSetLayout(),
        sizeof(rhi::TerrainPushConstants)
    );

    createCommandBuffers();
    initSyncObjects();
    initMultiBounceGI();
    initTexturesAndDescriptors();
    initHdrAndPostprocessPipelines();

    // Try loading Everest DEM data from Step 1; fallback to high-altitude procedural fractal terrain
    std::string demPath = DATA_DIR "/processed/everest_dem_float32.bin";
    std::string manifestPath = DATA_DIR "/processed/manifest.json";

    std::ifstream demTest(demPath);
    if (demTest.good()) {
        demTest.close();
        loadEverestDem(manifestPath, demPath, 1); // Full 1024x1024 = 1,048,576 vertices, 2,093,058 triangles!
    } else {
        generateTerrainMesh(256);
    }

    initMicroTerrainMesh();
    initBoulderMeshAndInstances();
}

Renderer::~Renderer() {
    VkDevice device = m_context->getDevice();
    vkDeviceWaitIdle(device);

    m_histogramPipeline.reset();
    m_adaptPipeline.reset();
    m_postprocessPipeline.reset();
    m_giComputePipeline.reset();
    m_histogramBuffer.reset();
    m_exposureBuffer.reset();
    m_microPipeline.reset();
    m_microVertexBuffer.reset();
    m_microIndexBuffer.reset();
    m_boulderPipeline.reset();
    m_boulderVertexBuffer.reset();
    m_boulderIndexBuffer.reset();
    m_boulderInstanceBuffer.reset();

    cleanupMultiBounceGI();

    if (m_histogramDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_histogramDescriptorLayout, nullptr);
    }
    if (m_adaptDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_adaptDescriptorLayout, nullptr);
    }
    if (m_postprocessDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_postprocessDescriptorLayout, nullptr);
    }

    if (m_hdrSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_hdrSampler, nullptr);
    }
    cleanupHdrResources();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, m_inFlightFences[i], nullptr);
    }
}

void Renderer::initSyncObjects() {
    VkDevice device = m_context->getDevice();

    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }
}

void Renderer::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_context->getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_context->getDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void Renderer::createHdrResources() {
    VkDevice device = m_context->getDevice();
    VkExtent2D extent = m_swapchain->getExtent();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_hdrImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create HDR color image!");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_hdrImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_context->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_hdrImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate HDR color image memory!");
    }

    vkBindImageMemory(device, m_hdrImage, m_hdrImageMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_hdrImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_hdrImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create HDR color image view!");
    }

    if (m_hdrSampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_hdrSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create HDR sampler!");
        }
    }
}

void Renderer::cleanupHdrResources() {
    VkDevice device = m_context->getDevice();
    if (m_hdrImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_hdrImageView, nullptr);
        m_hdrImageView = VK_NULL_HANDLE;
    }
    if (m_hdrImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_hdrImage, nullptr);
        m_hdrImage = VK_NULL_HANDLE;
    }
    if (m_hdrImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_hdrImageMemory, nullptr);
        m_hdrImageMemory = VK_NULL_HANDLE;
    }
}

void Renderer::initMultiBounceGI() {
    VkDevice device = m_context->getDevice();

    // 1. Create 512x512 R16G16B16A16_SFLOAT Image for Multi-Bounce Irradiance
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = GI_RES;
    imageInfo.extent.height = GI_RES;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_giImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Multi-Bounce GI image!");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_giImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_context->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_giImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Multi-Bounce GI image memory!");
    }

    vkBindImageMemory(device, m_giImage, m_giImageMemory, 0);

    // 2. Create Image View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_giImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_giImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Multi-Bounce GI image view!");
    }

    // 3. Create Sampler (Linear filtering, clamp to edge)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_giSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Multi-Bounce GI sampler!");
    }
}

void Renderer::cleanupMultiBounceGI() {
    VkDevice device = m_context->getDevice();
    if (m_giImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_giImageView, nullptr);
        m_giImageView = VK_NULL_HANDLE;
    }
    if (m_giImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_giImage, nullptr);
        m_giImage = VK_NULL_HANDLE;
    }
    if (m_giImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_giImageMemory, nullptr);
        m_giImageMemory = VK_NULL_HANDLE;
    }
    if (m_giSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_giSampler, nullptr);
        m_giSampler = VK_NULL_HANDLE;
    }
    if (m_giDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_giDescriptorLayout, nullptr);
        m_giDescriptorLayout = VK_NULL_HANDLE;
    }
}

void Renderer::dispatchMultiBounceGI(
    VkCommandBuffer cmd,
    const glm::vec3& sunDir,
    const glm::vec3& sunColor,
    const glm::vec3& cameraPos,
    float time,
    float windSpeed,
    float blizzardFactor
) {
    if (!m_giComputePipeline || m_giDescriptorSet == VK_NULL_HANDLE) return;

    // 1. Transition GI Image to GENERAL for Compute Write
    VkImageMemoryBarrier barrierToGen{};
    barrierToGen.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToGen.oldLayout = m_giCurrentLayout;
    barrierToGen.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierToGen.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGen.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGen.image = m_giImage;
    barrierToGen.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierToGen.subresourceRange.baseMipLevel = 0;
    barrierToGen.subresourceRange.levelCount = 1;
    barrierToGen.subresourceRange.baseArrayLayer = 0;
    barrierToGen.subresourceRange.layerCount = 1;
    barrierToGen.srcAccessMask = (m_giCurrentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ? VK_ACCESS_SHADER_READ_BIT : 0;
    barrierToGen.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    VkPipelineStageFlags srcStage = (m_giCurrentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrierToGen
    );
    m_giCurrentLayout = VK_IMAGE_LAYOUT_GENERAL;

    // 2. Bind Compute Pipeline & Descriptor Set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_giComputePipeline->getHandle());
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_giComputePipeline->getLayout(),
        0, 1, &m_giDescriptorSet,
        0, nullptr
    );

    // 3. Push Constants
    struct GIPushConstants {
        glm::vec4 sunDir;         // xyz = normalized sun direction, w = windSpeed
        glm::vec4 sunColor;       // rgb = sun radiance spectrum, w = blizzardFactor
        glm::vec4 cameraPos;      // xyz = camera world pos, w = time
        glm::vec4 cwmCenter;      // xy = Western Cwm center XZ (-12500.0, -8200.0), z = innerRadius (1800.0), w = outerRadius (3800.0)
        uint32_t bounceIndex;      // 0..8
        uint32_t totalBounces;     // 8
        float minElev;         // 3651.0m
        float maxElev;         // 8780.0m
    } pc{};

    glm::vec3 normSun = glm::normalize(sunDir);
    pc.sunDir = glm::vec4(normSun, windSpeed);
    pc.sunColor = glm::vec4(sunColor, blizzardFactor);
    pc.cameraPos = glm::vec4(cameraPos, time);
    pc.cwmCenter = glm::vec4(-12000.0f, -7600.0f, 2200.0f, 4500.0f);
    pc.bounceIndex = 0;
    pc.totalBounces = 8;
    pc.minElev = m_minElevation;
    pc.maxElev = m_maxElevation;

    vkCmdPushConstants(
        cmd,
        m_giComputePipeline->getLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(GIPushConstants),
        &pc
    );

    // 4. Dispatch Compute Workgroups (512 / 8 = 64)
    uint32_t groupCount = GI_RES / 8;
    vkCmdDispatch(cmd, groupCount, groupCount, 1);

    // 5. Transition GI Image to SHADER_READ_ONLY_OPTIMAL for Rasterization Fragment Shaders
    VkImageMemoryBarrier barrierToRead{};
    barrierToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToRead.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierToRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToRead.image = m_giImage;
    barrierToRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierToRead.subresourceRange.baseMipLevel = 0;
    barrierToRead.subresourceRange.levelCount = 1;
    barrierToRead.subresourceRange.baseArrayLayer = 0;
    barrierToRead.subresourceRange.layerCount = 1;
    barrierToRead.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrierToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrierToRead
    );
    m_giCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void Renderer::initTexturesAndDescriptors() {
    VkDevice device = m_context->getDevice();

    // 1. Create Descriptor Pool for textures (21 samplers) + postprocess/compute descriptors
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 16;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool for textures!");
    }

    // 2. Allocate Descriptor Set for Terrain Textures
    VkDescriptorSetLayout layout = m_pipeline->getDescriptorSetLayout();
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate terrain texture descriptor set!");
    }

    // 3. Load all 21 texture maps (ESRI satellite, Macro Normal, Geomorphology, 4x ambientCG CC0 PBR sets, DEM Float32, and Multi-Bounce GI)
    struct TexDef {
        std::string path;
        bool isSrgb;
        bool clamp;
    };

    std::vector<TexDef> texDefs = {
        // 0..1: Macro textures
        {DATA_DIR "/processed/everest_satellite_albedo.jpg", true, true},
        {DATA_DIR "/processed/everest_normal_map.png", false, true},

        // 2..5: Rock PBR
        {DATA_DIR "/textures/rock/albedo.jpg", true, false},
        {DATA_DIR "/textures/rock/normal.jpg", false, false},
        {DATA_DIR "/textures/rock/roughness.jpg", false, false},
        {DATA_DIR "/textures/rock/displacement.jpg", false, false},

        // 6..9: Snow PBR
        {DATA_DIR "/textures/snow/albedo.jpg", true, false},
        {DATA_DIR "/textures/snow/normal.jpg", false, false},
        {DATA_DIR "/textures/snow/roughness.jpg", false, false},
        {DATA_DIR "/textures/snow/displacement.jpg", false, false},

        // 10..13: Scree PBR
        {DATA_DIR "/textures/scree/albedo.jpg", true, false},
        {DATA_DIR "/textures/scree/normal.jpg", false, false},
        {DATA_DIR "/textures/scree/roughness.jpg", false, false},
        {DATA_DIR "/textures/scree/displacement.jpg", false, false},

        // 14..17: Glacier PBR
        {DATA_DIR "/textures/glacier/albedo.jpg", true, false},
        {DATA_DIR "/textures/glacier/normal.jpg", false, false},
        {DATA_DIR "/textures/glacier/roughness.jpg", false, false},
        {DATA_DIR "/textures/glacier/displacement.jpg", false, false},

        // 18: Geomorphology (R: Couloirs/Flow, G: Talus Scree, B: Ridge Crests, A: Wind Scour)
        {DATA_DIR "/processed/everest_geomorphology.png", false, true}
    };

    size_t totalTexCount = texDefs.size() + 2; // 19 PBR + DEM + MultiBounceGI = 21 textures
    m_textures.reserve(texDefs.size() + 1);
    std::vector<VkDescriptorImageInfo> imageInfos(totalTexCount);
    std::vector<VkWriteDescriptorSet> writes(totalTexCount);

    std::cout << "[Renderer] Loading 19 PBR, Geomorphology & Satellite textures into GPU VRAM..." << std::endl;
    for (size_t i = 0; i < texDefs.size(); i++) {
        m_textures.push_back(std::make_unique<rhi::VulkanTexture>(
            *m_context,
            texDefs[i].path,
            texDefs[i].isSrgb,
            texDefs[i].clamp
        ));
        imageInfos[i] = m_textures.back()->getDescriptorInfo();

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext = nullptr;
        writes[i].dstSet = m_descriptorSet;
        writes[i].dstBinding = static_cast<uint32_t>(i);
        writes[i].dstArrayElement = 0;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];
        writes[i].pBufferInfo = nullptr;
        writes[i].pTexelBufferView = nullptr;
    }

    // 19: 1024x1024 Float32 DEM Heightfield for GPU Cone-Tracing Shadows
    size_t demIdx = texDefs.size();
    std::string demPath = DATA_DIR "/processed/everest_dem_float32.bin";
    std::ifstream demFile(demPath, std::ios::binary);
    std::vector<float> demData(1024 * 1024, 0.0f);
    if (demFile.is_open()) {
        demFile.read(reinterpret_cast<char*>(demData.data()), demData.size() * sizeof(float));
        demFile.close();
        if (m_demElevations.empty()) m_demElevations = demData;
        std::cout << "[Renderer] Loaded 1024x1024 Float32 DEM ("
                  << (demData.size() * sizeof(float)) / 1048576
                  << " MB) into GPU VRAM for Cone-Tracing Shadows." << std::endl;
    } else {
        std::cerr << "[Renderer] Warning: Could not open DEM file " << demPath << " for Cone-Tracing!" << std::endl;
    }

    m_textures.push_back(std::make_unique<rhi::VulkanTexture>(
        *m_context,
        demData.data(),
        1024,
        1024,
        true // clampToEdge
    ));
    imageInfos[demIdx] = m_textures.back()->getDescriptorInfo();

    writes[demIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[demIdx].pNext = nullptr;
    writes[demIdx].dstSet = m_descriptorSet;
    writes[demIdx].dstBinding = static_cast<uint32_t>(demIdx);
    writes[demIdx].dstArrayElement = 0;
    writes[demIdx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[demIdx].descriptorCount = 1;
    writes[demIdx].pImageInfo = &imageInfos[demIdx];
    writes[demIdx].pBufferInfo = nullptr;
    writes[demIdx].pTexelBufferView = nullptr;

    // 20: Step 19 Hardware Ray-Traced Multi-Bounce GI Irradiance Texture (Western Cwm Glutofen)
    size_t giIdx = demIdx + 1;
    imageInfos[giIdx].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[giIdx].imageView = m_giImageView;
    imageInfos[giIdx].sampler = m_giSampler;

    writes[giIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[giIdx].pNext = nullptr;
    writes[giIdx].dstSet = m_descriptorSet;
    writes[giIdx].dstBinding = static_cast<uint32_t>(giIdx);
    writes[giIdx].dstArrayElement = 0;
    writes[giIdx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[giIdx].descriptorCount = 1;
    writes[giIdx].pImageInfo = &imageInfos[giIdx];
    writes[giIdx].pBufferInfo = nullptr;
    writes[giIdx].pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    std::cout << "[Renderer] Successfully bound all 21 textures (including 35-km DEM & Multi-Bounce GI) to Descriptor Set." << std::endl;

    // 4. Create Multi-Bounce GI Compute Pipeline Descriptor Set & Pipeline
    std::vector<VkDescriptorSetLayoutBinding> giBindings(4);
    giBindings[0].binding = 0;
    giBindings[0].descriptorCount = 1;
    giBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    giBindings[0].pImmutableSamplers = nullptr;
    giBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    giBindings[1].binding = 1;
    giBindings[1].descriptorCount = 1;
    giBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    giBindings[1].pImmutableSamplers = nullptr;
    giBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    giBindings[2].binding = 2;
    giBindings[2].descriptorCount = 1;
    giBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    giBindings[2].pImmutableSamplers = nullptr;
    giBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    giBindings[3].binding = 3;
    giBindings[3].descriptorCount = 1;
    giBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    giBindings[3].pImmutableSamplers = nullptr;
    giBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo giLayoutInfo{};
    giLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    giLayoutInfo.bindingCount = static_cast<uint32_t>(giBindings.size());
    giLayoutInfo.pBindings = giBindings.data();
    if (vkCreateDescriptorSetLayout(device, &giLayoutInfo, nullptr, &m_giDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Multi-Bounce GI descriptor set layout!");
    }

    VkDescriptorSetAllocateInfo giAllocInfo{};
    giAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    giAllocInfo.descriptorPool = m_descriptorPool;
    giAllocInfo.descriptorSetCount = 1;
    giAllocInfo.pSetLayouts = &m_giDescriptorLayout;
    if (vkAllocateDescriptorSets(device, &giAllocInfo, &m_giDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Multi-Bounce GI descriptor set!");
    }

    VkDescriptorImageInfo demDescInfo = m_textures[19]->getDescriptorInfo();
    VkDescriptorImageInfo morphDescInfo = m_textures[18]->getDescriptorInfo();
    VkDescriptorImageInfo normDescInfo = m_textures[1]->getDescriptorInfo();

    VkDescriptorImageInfo giStorageInfo{};
    giStorageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    giStorageInfo.imageView = m_giImageView;
    giStorageInfo.sampler = VK_NULL_HANDLE;

    std::vector<VkWriteDescriptorSet> giWrites(4);
    giWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    giWrites[0].dstSet = m_giDescriptorSet;
    giWrites[0].dstBinding = 0;
    giWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    giWrites[0].descriptorCount = 1;
    giWrites[0].pImageInfo = &demDescInfo;

    giWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    giWrites[1].dstSet = m_giDescriptorSet;
    giWrites[1].dstBinding = 1;
    giWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    giWrites[1].descriptorCount = 1;
    giWrites[1].pImageInfo = &morphDescInfo;

    giWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    giWrites[2].dstSet = m_giDescriptorSet;
    giWrites[2].dstBinding = 2;
    giWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    giWrites[2].descriptorCount = 1;
    giWrites[2].pImageInfo = &normDescInfo;

    giWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    giWrites[3].dstSet = m_giDescriptorSet;
    giWrites[3].dstBinding = 3;
    giWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    giWrites[3].descriptorCount = 1;
    giWrites[3].pImageInfo = &giStorageInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(giWrites.size()), giWrites.data(), 0, nullptr);

    m_giComputePipeline = std::make_unique<rhi::VulkanComputePipeline>(
        *m_context,
        SHADER_DIR "/multi_bounce_gi_comp.spv",
        m_giDescriptorLayout,
        sizeof(glm::vec4) * 4 + sizeof(uint32_t) * 2 + sizeof(float) * 2
    );
    std::cout << "[Renderer] Hardware Ray-Traced Multi-Bounce GI compute pipeline initialized." << std::endl;
}

void Renderer::initHdrAndPostprocessPipelines() {
    VkDevice device = m_context->getDevice();

    // 1. Create Histogram storage buffer (64 uints = 256 bytes)
    m_histogramBuffer = std::make_unique<rhi::VulkanBuffer>(
        *m_context,
        64 * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // 2. Create Exposure storage buffer (4 floats = 16 bytes: adaptedLum, exposure, targetLum, pad)
    m_exposureBuffer = std::make_unique<rhi::VulkanBuffer>(
        *m_context,
        4 * sizeof(float),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // Initial exposure values
    float initExp[4] = {1.0f, 1.0f, 1.0f, 0.0f};
    m_exposureBuffer->upload(initExp, sizeof(initExp));

    // 3. Create Histogram Descriptor Set Layout:
    // Binding 0: hdrTexture (COMBINED_IMAGE_SAMPLER, COMPUTE)
    // Binding 1: histogramBuffer (STORAGE_BUFFER, COMPUTE)
    std::vector<VkDescriptorSetLayoutBinding> histBindings(2);
    histBindings[0].binding = 0;
    histBindings[0].descriptorCount = 1;
    histBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    histBindings[0].pImmutableSamplers = nullptr;
    histBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    histBindings[1].binding = 1;
    histBindings[1].descriptorCount = 1;
    histBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    histBindings[1].pImmutableSamplers = nullptr;
    histBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo histLayoutInfo{};
    histLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    histLayoutInfo.bindingCount = static_cast<uint32_t>(histBindings.size());
    histLayoutInfo.pBindings = histBindings.data();
    if (vkCreateDescriptorSetLayout(device, &histLayoutInfo, nullptr, &m_histogramDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create histogram descriptor set layout!");
    }

    // 4. Create Adapt Exposure Descriptor Set Layout:
    // Binding 0: histogramBuffer (STORAGE_BUFFER, COMPUTE)
    // Binding 1: exposureBuffer (STORAGE_BUFFER, COMPUTE)
    std::vector<VkDescriptorSetLayoutBinding> adaptBindings(2);
    adaptBindings[0].binding = 0;
    adaptBindings[0].descriptorCount = 1;
    adaptBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    adaptBindings[0].pImmutableSamplers = nullptr;
    adaptBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    adaptBindings[1].binding = 1;
    adaptBindings[1].descriptorCount = 1;
    adaptBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    adaptBindings[1].pImmutableSamplers = nullptr;
    adaptBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo adaptLayoutInfo{};
    adaptLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    adaptLayoutInfo.bindingCount = static_cast<uint32_t>(adaptBindings.size());
    adaptLayoutInfo.pBindings = adaptBindings.data();
    if (vkCreateDescriptorSetLayout(device, &adaptLayoutInfo, nullptr, &m_adaptDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create adapt exposure descriptor set layout!");
    }

    // 5. Create Post-Process Descriptor Set Layout:
    // Binding 0: hdrTexture (COMBINED_IMAGE_SAMPLER, FRAGMENT)
    // Binding 1: exposureBuffer (STORAGE_BUFFER, FRAGMENT)
    std::vector<VkDescriptorSetLayoutBinding> postBindings(2);
    postBindings[0].binding = 0;
    postBindings[0].descriptorCount = 1;
    postBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postBindings[0].pImmutableSamplers = nullptr;
    postBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    postBindings[1].binding = 1;
    postBindings[1].descriptorCount = 1;
    postBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    postBindings[1].pImmutableSamplers = nullptr;
    postBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo postLayoutInfo{};
    postLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    postLayoutInfo.bindingCount = static_cast<uint32_t>(postBindings.size());
    postLayoutInfo.pBindings = postBindings.data();
    if (vkCreateDescriptorSetLayout(device, &postLayoutInfo, nullptr, &m_postprocessDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create post-process descriptor set layout!");
    }

    // Allocate descriptor sets
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;

    allocInfo.pSetLayouts = &m_histogramDescriptorLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &m_histogramDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate histogram descriptor set!");
    }

    allocInfo.pSetLayouts = &m_adaptDescriptorLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &m_adaptDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate adapt descriptor set!");
    }

    allocInfo.pSetLayouts = &m_postprocessDescriptorLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &m_postprocessDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate post-process descriptor set!");
    }

    // Bind resources to descriptor sets
    updateHdrDescriptorSets();

    // Create Compute Pipelines
    // Histogram push constants: width, height, minLogLum, maxLogLum (16 bytes)
    m_histogramPipeline = std::make_unique<rhi::VulkanComputePipeline>(
        *m_context,
        SHADER_DIR "/histogram_comp.spv",
        m_histogramDescriptorLayout,
        sizeof(uint32_t) * 2 + sizeof(float) * 2
    );

    // Adapt push constants: totalPixels, deltaTime, minLogLum, maxLogLum, adaptSpeedUp, adaptSpeedDown, exposureKey (28 bytes)
    m_adaptPipeline = std::make_unique<rhi::VulkanComputePipeline>(
        *m_context,
        SHADER_DIR "/adapt_exposure_comp.spv",
        m_adaptDescriptorLayout,
        sizeof(uint32_t) + sizeof(float) * 6
    );

    // Create Post-Process Graphics Pipeline
    std::string postVert = SHADER_DIR "/postprocess_vert.spv";
    std::string postFrag = SHADER_DIR "/postprocess_frag.spv";

    m_postprocessPipeline = std::make_unique<rhi::VulkanPipeline>(
        *m_context,
        m_swapchain->getImageFormat(),
        VK_FORMAT_UNDEFINED,
        postVert,
        postFrag,
        m_postprocessDescriptorLayout,
        sizeof(rhi::PostProcessPushConstants),
        true // isFullscreen
    );

    std::cout << "[Renderer] Photometric HDR & Human Eye Adaptation pipelines initialized." << std::endl;
}

void Renderer::updateHdrDescriptorSets() {
    VkDevice device = m_context->getDevice();

    VkDescriptorImageInfo hdrImageInfo{};
    hdrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrImageInfo.imageView = m_hdrImageView;
    hdrImageInfo.sampler = m_hdrSampler;

    VkDescriptorBufferInfo histBufferInfo{};
    histBufferInfo.buffer = m_histogramBuffer->getHandle();
    histBufferInfo.offset = 0;
    histBufferInfo.range = 64 * sizeof(uint32_t);

    VkDescriptorBufferInfo expBufferInfo{};
    expBufferInfo.buffer = m_exposureBuffer->getHandle();
    expBufferInfo.offset = 0;
    expBufferInfo.range = 4 * sizeof(float);

    std::vector<VkWriteDescriptorSet> writes;

    // 1. Histogram set writes
    VkWriteDescriptorSet histImageWrite{};
    histImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    histImageWrite.dstSet = m_histogramDescriptorSet;
    histImageWrite.dstBinding = 0;
    histImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    histImageWrite.descriptorCount = 1;
    histImageWrite.pImageInfo = &hdrImageInfo;
    writes.push_back(histImageWrite);

    VkWriteDescriptorSet histBufWrite{};
    histBufWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    histBufWrite.dstSet = m_histogramDescriptorSet;
    histBufWrite.dstBinding = 1;
    histBufWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    histBufWrite.descriptorCount = 1;
    histBufWrite.pBufferInfo = &histBufferInfo;
    writes.push_back(histBufWrite);

    // 2. Adapt set writes
    VkWriteDescriptorSet adaptHistWrite{};
    adaptHistWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    adaptHistWrite.dstSet = m_adaptDescriptorSet;
    adaptHistWrite.dstBinding = 0;
    adaptHistWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    adaptHistWrite.descriptorCount = 1;
    adaptHistWrite.pBufferInfo = &histBufferInfo;
    writes.push_back(adaptHistWrite);

    VkWriteDescriptorSet adaptExpWrite{};
    adaptExpWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    adaptExpWrite.dstSet = m_adaptDescriptorSet;
    adaptExpWrite.dstBinding = 1;
    adaptExpWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    adaptExpWrite.descriptorCount = 1;
    adaptExpWrite.pBufferInfo = &expBufferInfo;
    writes.push_back(adaptExpWrite);

    // 3. Post-process set writes
    VkWriteDescriptorSet postImageWrite{};
    postImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    postImageWrite.dstSet = m_postprocessDescriptorSet;
    postImageWrite.dstBinding = 0;
    postImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postImageWrite.descriptorCount = 1;
    postImageWrite.pImageInfo = &hdrImageInfo;
    writes.push_back(postImageWrite);

    VkWriteDescriptorSet postExpWrite{};
    postExpWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    postExpWrite.dstSet = m_postprocessDescriptorSet;
    postExpWrite.dstBinding = 1;
    postExpWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    postExpWrite.descriptorCount = 1;
    postExpWrite.pBufferInfo = &expBufferInfo;
    writes.push_back(postExpWrite);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void Renderer::onResize() {
    m_swapchain->recreate(m_window);
    cleanupHdrResources();
    createHdrResources();
    updateHdrDescriptorSets();
}

void Renderer::generateTerrainMesh(uint32_t gridResolution) {
    std::cout << "[Renderer] Generating fallback procedural fractal mountain mesh ("
              << gridResolution << "x" << gridResolution << ")..." << std::endl;

    std::vector<rhi::Vertex> vertices;
    std::vector<uint32_t> indices;

    float meshWidth = 24000.0f;
    float meshHeight = 24000.0f;
    float dx = meshWidth / (gridResolution - 1);
    float dz = meshHeight / (gridResolution - 1);

    vertices.reserve(gridResolution * gridResolution);

    for (uint32_t z = 0; z < gridResolution; z++) {
        for (uint32_t x = 0; x < gridResolution; x++) {
            float worldX = (static_cast<float>(x) / (gridResolution - 1) - 0.5f) * meshWidth;
            float worldZ = (static_cast<float>(z) / (gridResolution - 1) - 0.5f) * meshHeight;

            float r = std::sqrt(worldX * worldX + worldZ * worldZ) / (meshWidth * 0.5f);
            float dome = std::max(0.0f, 1.0f - r * r);
            float fbm = std::sin(worldX * 0.0012f) * std::cos(worldZ * 0.0012f) * 850.0f +
                        std::sin(worldX * 0.0028f + 1.2f) * std::cos(worldZ * 0.0028f) * 420.0f +
                        std::sin(worldX * 0.0065f) * std::cos(worldZ * 0.0065f + 0.8f) * 180.0f;

            float worldY = 3800.0f + dome * (4200.0f + fbm);

            rhi::Vertex v{};
            v.position = glm::vec3(worldX, worldY, worldZ);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.uv = glm::vec2(static_cast<float>(x) / (gridResolution - 1),
                             static_cast<float>(z) / (gridResolution - 1));
            vertices.push_back(v);
        }
    }

    // Indices
    for (uint32_t z = 0; z < gridResolution - 1; z++) {
        for (uint32_t x = 0; x < gridResolution - 1; x++) {
            uint32_t topLeft = z * gridResolution + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (z + 1) * gridResolution + x;
            uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    // Compute normals
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        glm::vec3 v0 = vertices[i0].position;
        glm::vec3 v1 = vertices[i1].position;
        glm::vec3 v2 = vertices[i2].position;

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        vertices[i0].normal += normal;
        vertices[i1].normal += normal;
        vertices[i2].normal += normal;
    }

    for (auto& v : vertices) {
        v.normal = glm::normalize(v.normal);
    }

    m_indexCount = static_cast<uint32_t>(indices.size());

    m_vertexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        vertices.data(),
        vertices.size() * sizeof(rhi::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    m_indexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        indices.data(),
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );

    std::cout << "[Renderer] Generated " << vertices.size() << " vertices, "
              << indices.size() / 3 << " triangles for Everest massif." << std::endl;
}

void Renderer::loadEverestDem(const std::string& manifestPath, const std::string& demBinPath, uint32_t sampleStep) {
    std::cout << "[Renderer] Loading Everest DEM from " << demBinPath << "..." << std::endl;

    std::ifstream demFile(demBinPath, std::ios::binary);
    if (!demFile.is_open()) {
        std::cerr << "[Renderer] Failed to open DEM binary file: " << demBinPath << ". Falling back to procedural terrain." << std::endl;
        generateTerrainMesh(256);
        return;
    }

    std::vector<float> demData(1024 * 1024);
    demFile.read(reinterpret_cast<char*>(demData.data()), demData.size() * sizeof(float));
    demFile.close();
    m_demElevations = demData;

    uint32_t demWidth = 1024;
    uint32_t demHeight = 1024;

    float meshWidth = 34560.0f;
    float meshDepth = 34560.0f;

    uint32_t gridW = (demWidth - 1) / sampleStep + 1;
    uint32_t gridH = (demHeight - 1) / sampleStep + 1;

    std::vector<rhi::Vertex> vertices;
    vertices.reserve(gridW * gridH);

    float minZ = 10000.0f, maxZ = -10000.0f;

    for (uint32_t gz = 0; gz < gridH; gz++) {
        uint32_t demY = gz * sampleStep;
        for (uint32_t gx = 0; gx < gridW; gx++) {
            uint32_t demX = gx * sampleStep;

            float elevation = demData[demY * demWidth + demX];
            minZ = std::min(minZ, elevation);
            maxZ = std::max(maxZ, elevation);

            float worldX = (static_cast<float>(demX) / (demWidth - 1) - 0.5f) * meshWidth;
            float worldZ = (static_cast<float>(demY) / (demHeight - 1) - 0.5f) * meshDepth;
            float worldY = elevation;

            rhi::Vertex v{};
            v.position = glm::vec3(worldX, worldY, worldZ);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.uv = glm::vec2(static_cast<float>(demX) / (demWidth - 1),
                             static_cast<float>(demY) / (demHeight - 1));
            vertices.push_back(v);
        }
    }

    m_minElevation = minZ;
    m_maxElevation = maxZ;
    std::cout << "[Renderer] Loaded Mount Everest 1:1 DEM into GPU ("
              << vertices.size() << " vertices, " << (gridW - 1) * (gridH - 1) * 2 << " triangles)." << std::endl;
    std::cout << "[Renderer] Elevation: " << minZ << " m to " << maxZ << " m." << std::endl;

    std::vector<uint32_t> indices;
    indices.reserve((gridW - 1) * (gridH - 1) * 6);

    for (uint32_t gz = 0; gz < gridH - 1; gz++) {
        for (uint32_t gx = 0; gx < gridW - 1; gx++) {
            uint32_t topLeft = gz * gridW + gx;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (gz + 1) * gridW + gx;
            uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    // Compute surface normals
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        glm::vec3 v0 = vertices[i0].position;
        glm::vec3 v1 = vertices[i1].position;
        glm::vec3 v2 = vertices[i2].position;

        glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        vertices[i0].normal += normal;
        vertices[i1].normal += normal;
        vertices[i2].normal += normal;
    }

    for (auto& v : vertices) {
        v.normal = glm::normalize(v.normal);
    }

    m_indexCount = static_cast<uint32_t>(indices.size());

    m_vertexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        vertices.data(),
        vertices.size() * sizeof(rhi::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    m_indexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        indices.data(),
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );
}

float Renderer::getDemElevation(float worldX, float worldZ) const {
    if (m_demElevations.empty()) return 5000.0f;
    float meshWidth = 34560.0f;
    float meshDepth = 34560.0f;
    uint32_t demWidth = 1024;
    uint32_t demHeight = 1024;

    float u = std::clamp((worldX / meshWidth) + 0.5f, 0.0f, 1.0f);
    float v = std::clamp((worldZ / meshDepth) + 0.5f, 0.0f, 1.0f);

    float gx = u * static_cast<float>(demWidth - 1);
    float gz = v * static_cast<float>(demHeight - 1);

    uint32_t x0 = static_cast<uint32_t>(std::floor(gx));
    uint32_t z0 = static_cast<uint32_t>(std::floor(gz));
    uint32_t x1 = std::min(x0 + 1, demWidth - 1);
    uint32_t z1 = std::min(z0 + 1, demHeight - 1);

    float tx = gx - static_cast<float>(x0);
    float tz = gz - static_cast<float>(z0);

    float h00 = m_demElevations[z0 * demWidth + x0];
    float h10 = m_demElevations[z0 * demWidth + x1];
    float h01 = m_demElevations[z1 * demWidth + x0];
    float h11 = m_demElevations[z1 * demWidth + x1];

    float h0 = h00 * (1.0f - tx) + h10 * tx;
    float h1 = h01 * (1.0f - tx) + h11 * tx;

    return h0 * (1.0f - tz) + h1 * tz;
}

glm::vec3 Renderer::getDemNormal(float worldX, float worldZ) const {
    constexpr float eps = 4.0f;
    float hL = getDemElevation(worldX - eps, worldZ);
    float hR = getDemElevation(worldX + eps, worldZ);
    float hD = getDemElevation(worldX, worldZ - eps);
    float hU = getDemElevation(worldX, worldZ + eps);

    float dx = (hR - hL) / (2.0f * eps);
    float dz = (hU - hD) / (2.0f * eps);

    return glm::normalize(glm::vec3(-dx, 1.0f, -dz));
}

void Renderer::initMicroTerrainMesh() {
    std::cout << "[Renderer] Generating 0.25m CDLOD Micro-Terrain Grid (80m x 80m, 320x320 quads)..." << std::endl;
    constexpr uint32_t quads = 320;
    constexpr uint32_t vertsPerSide = quads + 1;
    constexpr float spacing = 0.25f;
    constexpr float extent = 40.0f;

    std::vector<rhi::Vertex> vertices;
    vertices.reserve(vertsPerSide * vertsPerSide);

    for (uint32_t gz = 0; gz < vertsPerSide; gz++) {
        float localZ = static_cast<float>(gz) * spacing - extent;
        float v = static_cast<float>(gz) / static_cast<float>(quads);
        for (uint32_t gx = 0; gx < vertsPerSide; gx++) {
            float localX = static_cast<float>(gx) * spacing - extent;
            float u = static_cast<float>(gx) / static_cast<float>(quads);

            rhi::Vertex vert{};
            vert.position = glm::vec3(localX, 0.0f, localZ);
            vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vert.uv = glm::vec2(u, v);
            vertices.push_back(vert);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(quads * quads * 6);

    for (uint32_t gz = 0; gz < quads; gz++) {
        for (uint32_t gx = 0; gx < quads; gx++) {
            uint32_t topLeft = gz * vertsPerSide + gx;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (gz + 1) * vertsPerSide + gx;
            uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    m_microIndexCount = static_cast<uint32_t>(indices.size());

    m_microVertexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        vertices.data(),
        vertices.size() * sizeof(rhi::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    m_microIndexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        indices.data(),
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );

    std::cout << "[Renderer] CDLOD Micro-Terrain initialized ("
              << vertices.size() << " vertices, " << (indices.size() / 3)
              << " triangles @ 0.25m resolution)." << std::endl;
}

void Renderer::initBoulderMeshAndInstances() {
    std::cout << "[Renderer] Initializing 3D Frost-Shattered Boulder Mesh & Instance Buffer..." << std::endl;

    // Procedural faceted crystalline rock mesh (frost-split boulder)
    std::vector<glm::vec3> rawVerts = {
        // Top cap
        { 0.0f,  0.95f,  0.0f},
        // Upper ring
        {-0.55f, 0.72f, -0.45f},
        { 0.48f, 0.68f, -0.52f},
        { 0.62f, 0.70f,  0.42f},
        {-0.42f, 0.75f,  0.58f},
        // Mid-upper ring
        {-0.88f, 0.25f, -0.25f},
        {-0.22f, 0.32f, -0.85f},
        { 0.65f, 0.22f, -0.68f},
        { 0.92f, 0.28f,  0.18f},
        { 0.45f, 0.30f,  0.82f},
        {-0.48f, 0.22f,  0.78f},
        // Mid-lower ring
        {-0.82f, -0.32f, -0.35f},
        {-0.15f, -0.28f, -0.82f},
        { 0.58f, -0.35f, -0.58f},
        { 0.85f, -0.25f,  0.22f},
        { 0.38f, -0.32f,  0.75f},
        {-0.52f, -0.38f,  0.68f},
        // Bottom cap ring
        {-0.42f, -0.78f, -0.32f},
        { 0.35f, -0.75f, -0.38f},
        { 0.42f, -0.82f,  0.32f},
        {-0.35f, -0.80f,  0.38f},
        // Bottom apex
        { 0.0f,  -0.92f,  0.0f}
    };

    std::vector<uint32_t> rawIndices = {
        // Top fan
        0, 1, 2,   0, 2, 3,   0, 3, 4,   0, 4, 1,
        // Upper to mid-upper
        1, 5, 6,   1, 6, 2,   2, 6, 7,   2, 7, 3,   3, 7, 8,   3, 8, 9,   3, 9, 4,   4, 9, 10,  4, 10, 5,  4, 5, 1,
        // Mid-upper to mid-lower
        5, 11, 12, 5, 12, 6,  6, 12, 13, 6, 13, 7,  7, 13, 14, 7, 14, 8,  8, 14, 15, 8, 15, 9,  9, 15, 16, 9, 16, 10, 10, 16, 11, 10, 11, 5,
        // Mid-lower to bottom cap ring
        11, 17, 18, 11, 18, 12, 12, 18, 13, 13, 18, 19, 13, 19, 14, 14, 19, 15, 15, 19, 20, 15, 20, 16, 16, 20, 17, 16, 17, 11,
        // Bottom fan (wound so normals point outwards/downwards)
        21, 17, 18,   21, 18, 19,   21, 19, 20,   21, 20, 17
    };

    std::vector<rhi::Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(rawIndices.size());
    indices.reserve(rawIndices.size());

    for (size_t i = 0; i < rawIndices.size(); i += 3) {
        glm::vec3 p0 = rawVerts[rawIndices[i]];
        glm::vec3 p1 = rawVerts[rawIndices[i + 1]];
        glm::vec3 p2 = rawVerts[rawIndices[i + 2]];

        glm::vec3 centroid = (p0 + p1 + p2) / 3.0f;
        glm::vec3 faceNorm = glm::cross(p1 - p0, p2 - p0);
        if (glm::dot(faceNorm, centroid) < 0.0f) {
            std::swap(p1, p2);
            faceNorm = -faceNorm;
        }
        faceNorm = glm::normalize(faceNorm);

        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

        rhi::Vertex v0{}, v1{}, v2{};
        v0.position = p0; v0.normal = faceNorm; v0.uv = glm::vec2(p0.x, p0.z) * 0.5f + 0.5f;
        v1.position = p1; v1.normal = faceNorm; v1.uv = glm::vec2(p1.x, p1.z) * 0.5f + 0.5f;
        v2.position = p2; v2.normal = faceNorm; v2.uv = glm::vec2(p2.x, p2.z) * 0.5f + 0.5f;

        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);

        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
    }

    m_boulderIndexCount = static_cast<uint32_t>(indices.size());

    m_boulderVertexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        vertices.data(),
        vertices.size() * sizeof(rhi::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    m_boulderIndexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        indices.data(),
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );

    // Host-visible instance buffer for up to 1024 boulders
    m_boulderInstanceBuffer = std::make_unique<rhi::VulkanBuffer>(
        *m_context,
        1024 * sizeof(BoulderInstanceData),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    std::cout << "[Renderer] Boulder mesh ready (" << (indices.size() / 3) << " facets, capacity: 1024 instances)." << std::endl;
}

void Renderer::updateBoulderInstances(const glm::vec3& camPos) {
    if (!m_boulderInstanceBuffer || m_demElevations.empty()) return;

    constexpr float gridStep = 4.8f;
    int baseCX = static_cast<int>(std::floor(camPos.x / gridStep));
    int baseCZ = static_cast<int>(std::floor(camPos.z / gridStep));

    std::vector<BoulderInstanceData> instances;
    instances.reserve(768);

    for (int dz = -14; dz <= 14; dz++) {
        for (int dx = -14; dx <= 14; dx++) {
            int cx = baseCX + dx;
            int cz = baseCZ + dz;

            // Deterministic spatial hash
            uint32_t seed = static_cast<uint32_t>(cx * 73856093 ^ cz * 19349663);
            auto hashFloat = [](uint32_t& s) {
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                return static_cast<float>(s & 0xFFFF) / 65535.0f;
            };

            float jitterX = (hashFloat(seed) - 0.5f) * gridStep * 0.85f;
            float jitterZ = (hashFloat(seed) - 0.5f) * gridStep * 0.85f;

            float worldX = (static_cast<float>(cx) + 0.5f) * gridStep + jitterX;
            float worldZ = (static_cast<float>(cz) + 0.5f) * gridStep + jitterZ;

            float dCam = glm::length(glm::vec2(worldX - camPos.x, worldZ - camPos.z));
            // Keep clearance around player stance and limit outer radius
            if (dCam < 3.5f || dCam > 54.0f) continue;

            float elev = getDemElevation(worldX, worldZ);
            glm::vec3 norm = getDemNormal(worldX, worldZ);
            float slopeDeg = glm::degrees(std::acos(std::clamp(norm.y, 0.0f, 1.0f)));

            if (slopeDeg > 34.0f) continue; // Loose boulders tumble off steep faces, resting only on scree cones & moraines <= 34°

            // Spawn probability based on alpine terrain geomorphology
            float spawnRoll = hashFloat(seed);
            float spawnProb = 0.12f;
            if (slopeDeg >= 20.0f && slopeDeg <= 34.0f) {
                spawnProb = 0.42f; // Talus scree fan
            }
            if (elev >= 5200.0f && elev <= 5500.0f) {
                spawnProb = 0.36f; // Khumbu Base Camp moraine
            }
            if (elev >= 7800.0f && elev <= 8050.0f && slopeDeg < 25.0f) {
                spawnProb = 0.32f; // South Col high plateau
            }

            if (spawnRoll > spawnProb) continue;

            // Scale: 0.45m to 2.6m
            float baseScale = 0.45f + std::pow(hashFloat(seed), 2.2f) * 2.15f;
            float scaleX = baseScale * (0.8f + hashFloat(seed) * 0.4f);
            float scaleY = baseScale * (0.6f + hashFloat(seed) * 0.5f);
            float scaleZ = baseScale * (0.8f + hashFloat(seed) * 0.4f);

            // Naturally embed into ground (bottom half buried in snow / gravel)
            float embedY = scaleY * 0.52f;
            float posY = elev - embedY;

            // Random rotation
            float yaw = hashFloat(seed) * 6.283185f;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(worldX, posY, worldZ));
            model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(scaleX, scaleY, scaleZ));

            BoulderInstanceData inst{};
            inst.model = model; // Column-major GLM matrix mapped to vertex attributes

            // Lichen density: higher on sheltered rock faces between 4,800m and 8,300m
            float lichenAlt = (elev >= 4800.0f && elev <= 8300.0f) ? 1.0f : 0.0f;
            inst.params.x = (slopeDeg > 20.0f ? 0.85f : 0.35f) * lichenAlt * hashFloat(seed);

            // Rock formation: 0 = granite basement, 1 = Qomolangma limestone, 2 = Yellow Band
            float strataElev = elev - 0.22f * worldZ;
            if (strataElev >= 8600.0f) inst.params.y = 1.0f;
            else if (strataElev >= 8180.0f) inst.params.y = 2.0f;
            else inst.params.y = 0.0f;

            // Snow dusting
            inst.params.z = (elev > 5050.0f) ? (0.6f + hashFloat(seed) * 0.4f) : 0.15f;
            inst.params.w = baseScale;

            instances.push_back(inst);
            if (instances.size() >= 1000) break;
        }
        if (instances.size() >= 1000) break;
    }

    m_boulderInstanceCount = static_cast<uint32_t>(instances.size());
    if (m_boulderInstanceCount > 0) {
        m_boulderInstanceBuffer->upload(instances.data(), instances.size() * sizeof(BoulderInstanceData));
    }
}

void Renderer::renderFrame(
    const core::Camera& camera,
    float totalTime,
    const glm::vec3& sunDir,
    const glm::vec3& sunColor,
    float cloudDensity,
    float cloudBase,
    float blizzardFactor,
    float windSpeed
) {
    VkDevice device = m_context->getDevice();

    vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device,
        m_swapchain->getHandle(),
        UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        onResize();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    m_lastPresentedImage = imageIndex;
    vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    // Step 17: Read back adapted exposure telemetry from GPU storage buffer
    float deltaTime = (totalTime > m_lastFrameTime && m_lastFrameTime > 0.0f) ? (totalTime - m_lastFrameTime) : 0.016f;
    m_lastFrameTime = totalTime;

    void* mappedMem = nullptr;
    if (m_exposureBuffer && vkMapMemory(device, m_exposureBuffer->getMemory(), 0, 4 * sizeof(float), 0, &mappedMem) == VK_SUCCESS) {
        float* expData = static_cast<float*>(mappedMem);
        if (expData[0] > 0.0001f && !std::isnan(expData[0])) {
            m_adaptedLuminance = expData[0];
            m_currentExposure = expData[1];
            m_targetLuminance = expData[2];
        }
        vkUnmapMemory(device, m_exposureBuffer->getMemory());
    }

    // Project Sun onto screen coordinates for Hexagonal Sunstars & Anamorphic Glare
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    glm::vec3 sunNorm = glm::normalize(sunDir);
    glm::vec3 sunWorld = camera.getPosition() + sunNorm * 10000.0f;
    glm::vec4 sunClip = proj * (view * glm::vec4(sunWorld, 1.0f));

    glm::vec4 sunScreen(0.0f);
    if (sunClip.w > 0.0f) {
        glm::vec3 sunNDC = glm::vec3(sunClip) / sunClip.w;
        if (sunNDC.z >= 0.0f) {
            sunScreen.x = sunNDC.x * 0.5f + 0.5f;
            sunScreen.y = sunNDC.y * 0.5f + 0.5f;
            sunScreen.z = 1.0f; // in front of camera
            float sunElev = std::max(0.0f, sunNorm.y);
            sunScreen.w = glm::mix(0.85f, 1.4f, sunElev);
        }
    }

    // Step 18: Update dynamic boulder instances around camera
    updateBoulderInstances(camera.getPosition());

    // Step 19: Hardware Ray-Traced Multi-Bounce GI ("Das Glutofen-Schneelicht")
    dispatchMultiBounceGI(
        cmd,
        sunDir,
        sunColor,
        camera.getPosition(),
        totalTime,
        windSpeed,
        blizzardFactor
    );

    VkExtent2D extent = m_swapchain->getExtent();

    // =========================================================================
    // 1. Transition HDR Render Target to COLOR_ATTACHMENT_OPTIMAL
    // =========================================================================
    VkImageMemoryBarrier hdrAttachBarrier{};
    hdrAttachBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    hdrAttachBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    hdrAttachBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    hdrAttachBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hdrAttachBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hdrAttachBarrier.image = m_hdrImage;
    hdrAttachBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    hdrAttachBarrier.subresourceRange.baseMipLevel = 0;
    hdrAttachBarrier.subresourceRange.levelCount = 1;
    hdrAttachBarrier.subresourceRange.baseArrayLayer = 0;
    hdrAttachBarrier.subresourceRange.layerCount = 1;
    hdrAttachBarrier.srcAccessMask = 0;
    hdrAttachBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &hdrAttachBarrier
    );

    // =========================================================================
    // 2. Pass 1: Render Scene (Sky & Terrain) into 16-Bit Half-Float HDR Buffer
    // =========================================================================
    VkRenderingAttachmentInfo hdrColorAttachment{};
    hdrColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    hdrColorAttachment.imageView = m_hdrImageView;
    hdrColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    hdrColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    hdrColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    float camAltRatio = std::clamp((camera.getPosition().y - 4500.0f) / 4348.0f, 0.0f, 1.0f);
    glm::vec3 valleySky(0.35f, 0.52f, 0.72f);
    glm::vec3 stratosphericSky(0.0035f, 0.0065f, 0.024f);
    glm::vec3 clearSky = glm::mix(valleySky, stratosphericSky, camAltRatio * 0.90f);
    if (blizzardFactor > 0.01f) {
        clearSky = glm::mix(clearSky, glm::vec3(0.82f, 0.86f, 0.90f), blizzardFactor * 0.88f);
    }
    hdrColorAttachment.clearValue.color = {{clearSky.r, clearSky.g, clearSky.b, 1.0f}};

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_swapchain->getDepthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo hdrRenderingInfo{};
    hdrRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    hdrRenderingInfo.renderArea.offset = {0, 0};
    hdrRenderingInfo.renderArea.extent = extent;
    hdrRenderingInfo.layerCount = 1;
    hdrRenderingInfo.colorAttachmentCount = 1;
    hdrRenderingInfo.pColorAttachments = &hdrColorAttachment;
    hdrRenderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(cmd, &hdrRenderingInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Push Constants for Slang Shaders (Shared by Sky & Terrain)
    rhi::TerrainPushConstants pushConstants{};
    pushConstants.model = glm::mat4(1.0f);
    pushConstants.view = view;
    pushConstants.proj = proj;
    pushConstants.cameraPos = glm::vec4(camera.getPosition(), cloudDensity);
    pushConstants.sunDir = glm::vec4(sunNorm, windSpeed);
    pushConstants.sunColor = glm::vec4(sunColor, blizzardFactor);
    pushConstants.time = totalTime;
    pushConstants.minElev = m_minElevation;
    pushConstants.maxElev = m_maxElevation;
    pushConstants.cloudBase = cloudBase;

    // A. Draw Stratospheric Dynamic Sky into HDR
    if (m_skyPipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyPipeline->getHandle());
        vkCmdPushConstants(
            cmd,
            m_skyPipeline->getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(rhi::TerrainPushConstants),
            &pushConstants
        );
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    // B. Draw Terrain Mesh into HDR
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());
    vkCmdPushConstants(
        cmd,
        m_pipeline->getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(rhi::TerrainPushConstants),
        &pushConstants
    );

    if (m_vertexBuffer && m_indexBuffer && m_indexCount > 0) {
        if (m_descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getLayout(), 0, 1, &m_descriptorSet, 0, nullptr);
        }

        VkBuffer vertexBuffers[] = {m_vertexBuffer->getHandle()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
    }

    // Step 18: Draw 0.25m CDLOD Micro-Terrain Mesh (Climbing Vicinity)
    if (m_microPipeline && m_microVertexBuffer && m_microIndexCount > 0) {
        float snapSpacing = 0.25f;
        float snapX = std::floor(camera.getPosition().x / snapSpacing) * snapSpacing;
        float snapZ = std::floor(camera.getPosition().z / snapSpacing) * snapSpacing;

        rhi::TerrainPushConstants microPush = pushConstants;
        microPush.model = glm::translate(glm::mat4(1.0f), glm::vec3(snapX, 0.0f, snapZ));

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_microPipeline->getHandle());
        vkCmdPushConstants(
            cmd,
            m_microPipeline->getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(rhi::TerrainPushConstants),
            &microPush
        );
        if (m_descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_microPipeline->getLayout(), 0, 1, &m_descriptorSet, 0, nullptr);
        }

        VkBuffer microVBs[] = {m_microVertexBuffer->getHandle()};
        VkDeviceSize microOffsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, microVBs, microOffsets);
        vkCmdBindIndexBuffer(cmd, m_microIndexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_microIndexCount, 1, 0, 0, 0);
    }

    // Step 18: Draw GPU-Instanced 3D Boulders & Talus Rocks
    if (m_boulderPipeline && m_boulderVertexBuffer && m_boulderInstanceBuffer && m_boulderInstanceCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_boulderPipeline->getHandle());
        vkCmdPushConstants(
            cmd,
            m_boulderPipeline->getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(rhi::TerrainPushConstants),
            &pushConstants
        );
        if (m_descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_boulderPipeline->getLayout(), 0, 1, &m_descriptorSet, 0, nullptr);
        }

        VkBuffer boulderVBs[] = {m_boulderVertexBuffer->getHandle(), m_boulderInstanceBuffer->getHandle()};
        VkDeviceSize boulderOffsets[] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, boulderVBs, boulderOffsets);
        vkCmdBindIndexBuffer(cmd, m_boulderIndexBuffer->getHandle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_boulderIndexCount, m_boulderInstanceCount, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);

    // =========================================================================
    // 3. Pipeline Barrier: Transition HDR Image to Compute Shader Read
    // =========================================================================
    VkImageMemoryBarrier hdrToComputeBarrier{};
    hdrToComputeBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    hdrToComputeBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    hdrToComputeBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrToComputeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hdrToComputeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hdrToComputeBarrier.image = m_hdrImage;
    hdrToComputeBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    hdrToComputeBarrier.subresourceRange.baseMipLevel = 0;
    hdrToComputeBarrier.subresourceRange.levelCount = 1;
    hdrToComputeBarrier.subresourceRange.baseArrayLayer = 0;
    hdrToComputeBarrier.subresourceRange.layerCount = 1;
    hdrToComputeBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    hdrToComputeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &hdrToComputeBarrier
    );

    // =========================================================================
    // 4. Compute Pass 1: 64-Bin Log-Luminance Histogram Evaluation
    // =========================================================================
    if (m_histogramPipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_histogramPipeline->getHandle());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_histogramPipeline->getLayout(), 0, 1, &m_histogramDescriptorSet, 0, nullptr);

        struct HistPush {
            uint32_t width;
            uint32_t height;
            float minLogLum;
            float maxLogLum;
        } histPush{extent.width, extent.height, -8.0f, 16.0f};

        vkCmdPushConstants(cmd, m_histogramPipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(histPush), &histPush);
        uint32_t groupX = (extent.width + 15) / 16;
        uint32_t groupY = (extent.height + 15) / 16;
        vkCmdDispatch(cmd, groupX, groupY, 1);
    }

    // Barrier: Wait for histogram compute writes before adaptation reduction
    VkBufferMemoryBarrier histBarrier{};
    histBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    histBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    histBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    histBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    histBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    histBarrier.buffer = m_histogramBuffer->getHandle();
    histBarrier.offset = 0;
    histBarrier.size = 64 * sizeof(uint32_t);

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        1, &histBarrier,
        0, nullptr
    );

    // =========================================================================
    // 5. Compute Pass 2: Asymmetric Human Eye Adaptation & Exposure Reduction
    // =========================================================================
    if (m_adaptPipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_adaptPipeline->getHandle());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_adaptPipeline->getLayout(), 0, 1, &m_adaptDescriptorSet, 0, nullptr);

        struct AdaptPush {
            uint32_t totalPixels;
            float deltaTime;
            float minLogLum;
            float maxLogLum;
            float adaptSpeedUp;
            float adaptSpeedDown;
            float exposureKey;
        } adaptPush{
            extent.width * extent.height,
            deltaTime,
            -8.0f,
            16.0f,
            3.8f, // Fast pupil constriction (dark to snow)
            1.2f, // Slower rhodopsin recovery (snow to crevasse shadow)
            1.15f // ISO 100 middle-grey exposure key
        };

        vkCmdPushConstants(cmd, m_adaptPipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(adaptPush), &adaptPush);
        vkCmdDispatch(cmd, 1, 1, 1);
    }

    // Barrier: Wait for exposure buffer write before fragment post-process
    VkBufferMemoryBarrier expBarrier{};
    expBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    expBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    expBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    expBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    expBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    expBarrier.buffer = m_exposureBuffer->getHandle();
    expBarrier.offset = 0;
    expBarrier.size = 4 * sizeof(float);

    VkImageMemoryBarrier swapchainAttachBarrier{};
    swapchainAttachBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainAttachBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainAttachBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    swapchainAttachBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainAttachBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainAttachBarrier.image = m_swapchain->getImage(imageIndex);
    swapchainAttachBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchainAttachBarrier.subresourceRange.baseMipLevel = 0;
    swapchainAttachBarrier.subresourceRange.levelCount = 1;
    swapchainAttachBarrier.subresourceRange.baseArrayLayer = 0;
    swapchainAttachBarrier.subresourceRange.layerCount = 1;
    swapchainAttachBarrier.srcAccessMask = 0;
    swapchainAttachBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkImageMemoryBarrier postImageBarriers[] = {swapchainAttachBarrier};

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        1, &expBarrier,
        1, postImageBarriers
    );

    // =========================================================================
    // 6. Pass 3: Photometric Post-Processing, Glare, Sunstars & ACES to Swapchain
    // =========================================================================
    VkRenderingAttachmentInfo swapchainColorAttachment{};
    swapchainColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    swapchainColorAttachment.imageView = m_swapchain->getImageView(imageIndex);
    swapchainColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    swapchainColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapchainColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo postRenderingInfo{};
    postRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    postRenderingInfo.renderArea.offset = {0, 0};
    postRenderingInfo.renderArea.extent = extent;
    postRenderingInfo.layerCount = 1;
    postRenderingInfo.colorAttachmentCount = 1;
    postRenderingInfo.pColorAttachments = &swapchainColorAttachment;
    postRenderingInfo.pDepthAttachment = nullptr;

    vkCmdBeginRendering(cmd, &postRenderingInfo);

    if (m_postprocessPipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postprocessPipeline->getHandle());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postprocessPipeline->getLayout(), 0, 1, &m_postprocessDescriptorSet, 0, nullptr);

        rhi::PostProcessPushConstants postPush{};
        postPush.sunScreenPos = sunScreen;
        postPush.resolution = glm::vec2(static_cast<float>(extent.width), static_cast<float>(extent.height));
        postPush.time = totalTime;
        postPush.blizzard = blizzardFactor;

        vkCmdPushConstants(cmd, m_postprocessPipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(postPush), &postPush);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    vkCmdEndRendering(cmd);

    // =========================================================================
    // 7. Transition Swapchain Image to PRESENT_SRC_KHR
    // =========================================================================
    VkImageMemoryBarrier presentBarrier{};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.image = m_swapchain->getImage(imageIndex);
    presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    presentBarrier.subresourceRange.baseMipLevel = 0;
    presentBarrier.subresourceRange.levelCount = 1;
    presentBarrier.subresourceRange.baseArrayLayer = 0;
    presentBarrier.subresourceRange.layerCount = 1;
    presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    presentBarrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &presentBarrier
    );

    vkEndCommandBuffer(cmd);

    // Submit to Graphics/Compute Queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present to Swapchain
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {m_swapchain->getHandle()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_context->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        onResize();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool Renderer::saveScreenshot(const std::string& filepath) {
    vkDeviceWaitIdle(m_context->getDevice());

    VkImage srcImage = m_swapchain->getImage(m_lastPresentedImage);
    uint32_t width = m_swapchain->getExtent().width;
    uint32_t height = m_swapchain->getExtent().height;
    VkDeviceSize imageSize = width * height * 4;

    rhi::VulkanBuffer stagingBuffer(
        *m_context,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkCommandBuffer cmd = m_context->beginSingleTimeCommands();

    // 1. Transition swapchain image from PRESENT_SRC_KHR to TRANSFER_SRC_OPTIMAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = srcImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // 2. Copy image to buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyImageToBuffer(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           stagingBuffer.getHandle(), 1, &region);

    // 3. Transition swapchain image back to PRESENT_SRC_KHR
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_context->endSingleTimeCommands(cmd);

    // 4. Map memory and write PNG
    VkDevice device = m_context->getDevice();
    void* mapped = nullptr;
    vkMapMemory(device, stagingBuffer.getMemory(), 0, imageSize, 0, &mapped);

    std::vector<uint8_t> pixels(imageSize);
    const uint8_t* srcPixels = static_cast<const uint8_t*>(mapped);

    bool isBgra = (m_swapchain->getImageFormat() == VK_FORMAT_B8G8R8A8_SRGB ||
                   m_swapchain->getImageFormat() == VK_FORMAT_B8G8R8A8_UNORM);

    for (size_t i = 0; i < imageSize; i += 4) {
        if (isBgra) {
            pixels[i + 0] = srcPixels[i + 2]; // R
            pixels[i + 1] = srcPixels[i + 1]; // G
            pixels[i + 2] = srcPixels[i + 0]; // B
            pixels[i + 3] = 255;              // A
        } else {
            pixels[i + 0] = srcPixels[i + 0];
            pixels[i + 1] = srcPixels[i + 1];
            pixels[i + 2] = srcPixels[i + 2];
            pixels[i + 3] = 255;
        }
    }

    vkUnmapMemory(device, stagingBuffer.getMemory());

    int result = stbi_write_png(filepath.c_str(), static_cast<int>(width), static_cast<int>(height), 4, pixels.data(), static_cast<int>(width * 4));
    if (result) {
        std::cout << "[Renderer] Screenshot saved to: " << filepath << " (" << width << "x" << height << ")" << std::endl;
        return true;
    } else {
        std::cerr << "[Renderer] Failed to write screenshot to: " << filepath << std::endl;
        return false;
    }
}

} // namespace whiteout::renderer
