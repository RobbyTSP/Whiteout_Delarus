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

    std::string vertSpv = SHADER_DIR "/terrain_vert.spv";
    std::string fragSpv = SHADER_DIR "/terrain_frag.spv";

    m_pipeline = std::make_unique<rhi::VulkanPipeline>(
        *m_context,
        m_swapchain->getImageFormat(),
        m_swapchain->getDepthFormat(),
        vertSpv,
        fragSpv
    );

    createCommandBuffers();
    initSyncObjects();
    initTexturesAndDescriptors();

    // Try loading Everest DEM data from Step 1; fallback to high-altitude procedural fractal terrain
    std::string demPath = DATA_DIR "/processed/everest_dem_float32.bin";
    std::string manifestPath = DATA_DIR "/processed/manifest.json";

    std::ifstream demTest(demPath);
    if (demTest.good()) {
        demTest.close();
        loadEverestDem(manifestPath, demPath, 2); // 512x512 subsample = 262,144 vertices
    } else {
        generateTerrainMesh(256);
    }
}

Renderer::~Renderer() {
    VkDevice device = m_context->getDevice();
    vkDeviceWaitIdle(device);

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

void Renderer::initTexturesAndDescriptors() {
    VkDevice device = m_context->getDevice();

    // 1. Create Descriptor Pool for 19 combined image samplers
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 19;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool for textures!");
    }

    // 2. Allocate Descriptor Set
    VkDescriptorSetLayout layout = m_pipeline->getDescriptorSetLayout();
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate terrain texture descriptor set!");
    }

    // 3. Load all 19 texture maps (ESRI satellite, Macro Normal, Geomorphology, and 4x ambientCG CC0 PBR sets)
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

    m_textures.reserve(texDefs.size());
    std::vector<VkDescriptorImageInfo> imageInfos(texDefs.size());
    std::vector<VkWriteDescriptorSet> writes(texDefs.size());

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

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    std::cout << "[Renderer] Successfully bound all 19 PBR, Geomorphology & Satellite textures to Descriptor Set." << std::endl;
}

void Renderer::onResize() {
    m_swapchain->recreate(m_window);
}

void Renderer::loadEverestDem(const std::string& manifestPath, const std::string& demBinPath, uint32_t sampleStep) {
    const uint32_t origW = 1024;
    const uint32_t origH = 1024;

    std::ifstream demFile(demBinPath, std::ios::binary);
    if (!demFile.is_open()) {
        std::cerr << "Warning: Could not open " << demBinPath << ", generating procedural terrain." << std::endl;
        generateTerrainMesh(256);
        return;
    }

    std::vector<float> rawElevation(origW * origH);
    demFile.read(reinterpret_cast<char*>(rawElevation.data()), rawElevation.size() * sizeof(float));
    demFile.close();

    uint32_t gridW = origW / sampleStep;
    uint32_t gridH = origH / sampleStep;
    uint32_t vertexCount = gridW * gridH;

    // Himalayan Everest dimensions: ~34.6 km width x 34.5 km depth
    float worldWidth = 34610.0f;
    float worldDepth = 34520.0f;

    std::vector<rhi::Vertex> vertices(vertexCount);
    m_minElevation = 10000.0f;
    m_maxElevation = -10000.0f;

    for (uint32_t y = 0; y < gridH; y++) {
        for (uint32_t x = 0; x < gridW; x++) {
            uint32_t origX = x * sampleStep;
            uint32_t origY = y * sampleStep;
            float elev = rawElevation[origY * origW + origX];

            if (elev < m_minElevation) m_minElevation = elev;
            if (elev > m_maxElevation) m_maxElevation = elev;

            float u = static_cast<float>(x) / static_cast<float>(gridW - 1);
            float v = static_cast<float>(y) / static_cast<float>(gridH - 1);

            float posX = (u - 0.5f) * worldWidth;
            float posZ = (v - 0.5f) * worldDepth;
            float posY = elev;

            uint32_t idx = y * gridW + x;
            vertices[idx].position = glm::vec3(posX, posY, posZ);
            vertices[idx].uv = glm::vec2(u, v);
            vertices[idx].normal = glm::vec3(0.0f, 1.0f, 0.0f); // Default up
        }
    }

    // Compute accurate vertex normals from surrounding neighbors
    for (uint32_t y = 1; y < gridH - 1; y++) {
        for (uint32_t x = 1; x < gridW - 1; x++) {
            uint32_t idx = y * gridW + x;
            float hL = vertices[idx - 1].position.y;
            float hR = vertices[idx + 1].position.y;
            float hD = vertices[(y - 1) * gridW + x].position.y;
            float hU = vertices[(y + 1) * gridW + x].position.y;

            float dx = (worldWidth / static_cast<float>(gridW - 1)) * 2.0f;
            float dz = (worldDepth / static_cast<float>(gridH - 1)) * 2.0f;

            glm::vec3 normal = glm::normalize(glm::vec3((hL - hR) * dz, dx * dz, (hD - hU) * dx));
            vertices[idx].normal = normal;
        }
    }

    // Build index buffer (Triangle list)
    std::vector<uint32_t> indices;
    indices.reserve((gridW - 1) * (gridH - 1) * 6);

    for (uint32_t y = 0; y < gridH - 1; y++) {
        for (uint32_t x = 0; x < gridW - 1; x++) {
            uint32_t i0 = y * gridW + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (y + 1) * gridW + x;
            uint32_t i3 = i2 + 1;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    m_indexCount = static_cast<uint32_t>(indices.size());

    // Upload to device-local GPU buffers
    VkDeviceSize vertexBufferSize = sizeof(rhi::Vertex) * vertices.size();
    m_vertexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        vertices.data(),
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    m_indexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        indices.data(),
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );

    std::cout << "[Renderer] Loaded Mount Everest 1:1 DEM into GPU ("
              << vertices.size() << " vertices, " << m_indexCount / 3 << " triangles)." << std::endl;
    std::cout << "[Renderer] Elevation: " << m_minElevation << " m to " << m_maxElevation << " m." << std::endl;
}

void Renderer::generateTerrainMesh(uint32_t gridResolution) {
    uint32_t gridW = gridResolution;
    uint32_t gridH = gridResolution;
    uint32_t vertexCount = gridW * gridH;

    float worldWidth = 35000.0f;
    float worldDepth = 35000.0f;

    std::vector<rhi::Vertex> vertices(vertexCount);
    m_minElevation = 4000.0f;
    m_maxElevation = 8848.0f;

    for (uint32_t y = 0; y < gridH; y++) {
        for (uint32_t x = 0; x < gridW; x++) {
            float u = static_cast<float>(x) / static_cast<float>(gridW - 1);
            float v = static_cast<float>(y) / static_cast<float>(gridH - 1);

            float posX = (u - 0.5f) * worldWidth;
            float posZ = (v - 0.5f) * worldDepth;

            // Procedural peak profile centered on summit
            float distFromCenter = std::sqrt(posX * posX + posZ * posZ) / 18000.0f;
            float peak = std::exp(-distFromCenter * distFromCenter * 3.0f);
            float noise = std::sin(posX * 0.0005f) * std::cos(posZ * 0.0005f) * 600.0f;
            float posY = m_minElevation + peak * (m_maxElevation - m_minElevation) + noise;

            uint32_t idx = y * gridW + x;
            vertices[idx].position = glm::vec3(posX, posY, posZ);
            vertices[idx].uv = glm::vec2(u, v);
            vertices[idx].normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve((gridW - 1) * (gridH - 1) * 6);
    for (uint32_t y = 0; y < gridH - 1; y++) {
        for (uint32_t x = 0; x < gridW - 1; x++) {
            uint32_t i0 = y * gridW + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (y + 1) * gridW + x;
            uint32_t i3 = i2 + 1;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    m_indexCount = static_cast<uint32_t>(indices.size());

    VkDeviceSize vertexBufferSize = sizeof(rhi::Vertex) * vertices.size();
    m_vertexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        vertices.data(),
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    m_indexBuffer = rhi::VulkanBuffer::createDeviceLocalBuffer(
        *m_context,
        indices.data(),
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );
}

void Renderer::renderFrame(const core::Camera& camera, float totalTime) {
    VkDevice device = m_context->getDevice();

    vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult acquireRes = m_swapchain->acquireNextImage(m_imageAvailableSemaphores[m_currentFrame], &imageIndex);

    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
        onResize();
        return;
    } else if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // 1. Transition swapchain color image to COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier colorBarrier{};
    colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.image = m_swapchain->getImage(imageIndex);
    colorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorBarrier.subresourceRange.baseMipLevel = 0;
    colorBarrier.subresourceRange.levelCount = 1;
    colorBarrier.subresourceRange.baseArrayLayer = 0;
    colorBarrier.subresourceRange.layerCount = 1;
    colorBarrier.srcAccessMask = 0;
    colorBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &colorBarrier
    );

    // 2. Set up Dynamic Rendering
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_swapchain->getImageView(imageIndex);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // Dynamic Himalayan Stratospheric Sky: Deep indigo/navy above 7000m
    float camAltRatio = std::clamp((camera.getPosition().y - 4500.0f) / 4348.0f, 0.0f, 1.0f);
    glm::vec3 valleySky(0.35f, 0.52f, 0.72f);
    glm::vec3 stratosphericSky(0.06f, 0.09f, 0.22f);
    glm::vec3 clearSky = glm::mix(valleySky, stratosphericSky, camAltRatio * 0.85f);
    colorAttachment.clearValue.color = {{clearSky.r, clearSky.g, clearSky.b, 1.0f}};

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_swapchain->getDepthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = m_swapchain->getExtent();
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    // Dynamic viewport & scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchain->getExtent().width);
    viewport.height = static_cast<float>(m_swapchain->getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain->getExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind Pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());

    // Push Constants for Slang Shader
    rhi::TerrainPushConstants pushConstants{};
    pushConstants.model = glm::mat4(1.0f);
    pushConstants.view = camera.getViewMatrix();
    pushConstants.proj = camera.getProjectionMatrix();
    pushConstants.cameraPos = glm::vec4(camera.getPosition(), 1.0f);
    pushConstants.sunDir = glm::vec4(glm::normalize(glm::vec3(0.4f, 0.75f, 0.45f)), 0.0f);
    pushConstants.sunColor = glm::vec4(1.25f, 1.18f, 1.05f, 1.0f);
    pushConstants.time = totalTime;
    pushConstants.minElev = m_minElevation;
    pushConstants.maxElev = m_maxElevation;
    pushConstants.padding = 0.0f;

    vkCmdPushConstants(
        cmd,
        m_pipeline->getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(rhi::TerrainPushConstants),
        &pushConstants
    );

    // Draw Terrain Mesh
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

    vkCmdEndRendering(cmd);

    // Transition swapchain color image from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR
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

    // Submit to Graphics Queue
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

    // Present to Window
    VkResult presentRes = m_swapchain->present(m_renderFinishedSemaphores[m_currentFrame], imageIndex);
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
        onResize();
    } else if (presentRes != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image!");
    }

    m_lastPresentedImage = imageIndex;
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool Renderer::saveScreenshot(const std::string& filepath) {
    vkDeviceWaitIdle(m_context->getDevice());

    uint32_t width = m_swapchain->getExtent().width;
    uint32_t height = m_swapchain->getExtent().height;
    VkDeviceSize imageSize = width * height * 4;

    VkImage srcImage = m_swapchain->getImage(m_lastPresentedImage);

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
