#pragma once

#include "../core/Window.hpp"
#include "../core/Camera.hpp"
#include "../rhi/VulkanContext.hpp"
#include "../rhi/VulkanSwapchain.hpp"
#include "../rhi/VulkanPipeline.hpp"
#include "../rhi/VulkanComputePipeline.hpp"
#include "../rhi/VulkanBuffer.hpp"
#include "../rhi/VulkanTexture.hpp"
#include <memory>
#include <vector>

namespace whiteout::renderer {

class Renderer {
public:
    explicit Renderer(core::Window& window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void renderFrame(
        const core::Camera& camera,
        float totalTime,
        const glm::vec3& sunDir = glm::vec3(0.4f, 0.75f, 0.45f),
        const glm::vec3& sunColor = glm::vec3(1.30f, 1.25f, 1.15f),
        float cloudDensity = 0.75f,
        float cloudBase = 4950.0f,
        float blizzardFactor = 0.0f,
        float windSpeed = 30.0f
    );
    void onResize();
    bool saveScreenshot(const std::string& filepath);

    [[nodiscard]] const rhi::VulkanContext& getContext() const { return *m_context; }

    // Step 17: Photometric HDR Telemetry
    [[nodiscard]] float getAdaptedLuminance() const { return m_adaptedLuminance; }
    [[nodiscard]] float getCurrentExposure() const { return m_currentExposure; }
    [[nodiscard]] float getTargetLuminance() const { return m_targetLuminance; }

private:
    void initSyncObjects();
    void createCommandBuffers();
    void generateTerrainMesh(uint32_t gridResolution = 256);
    void loadEverestDem(const std::string& manifestPath, const std::string& demBinPath, uint32_t sampleStep = 4);

    void createHdrResources();
    void cleanupHdrResources();
    void initHdrAndPostprocessPipelines();
    void updateHdrDescriptorSets();

    core::Window& m_window;
    std::unique_ptr<rhi::VulkanContext> m_context;
    std::unique_ptr<rhi::VulkanSwapchain> m_swapchain;
    std::unique_ptr<rhi::VulkanPipeline> m_pipeline;
    std::unique_ptr<rhi::VulkanPipeline> m_skyPipeline;

    // Step 17: Photometric HDR Render Target & Eye Adaptation Pipelines
    VkImage m_hdrImage = VK_NULL_HANDLE;
    VkDeviceMemory m_hdrImageMemory = VK_NULL_HANDLE;
    VkImageView m_hdrImageView = VK_NULL_HANDLE;
    VkSampler m_hdrSampler = VK_NULL_HANDLE;

    std::unique_ptr<rhi::VulkanBuffer> m_histogramBuffer;
    std::unique_ptr<rhi::VulkanBuffer> m_exposureBuffer;

    std::unique_ptr<rhi::VulkanComputePipeline> m_histogramPipeline;
    std::unique_ptr<rhi::VulkanComputePipeline> m_adaptPipeline;
    std::unique_ptr<rhi::VulkanPipeline> m_postprocessPipeline;

    VkDescriptorSetLayout m_histogramDescriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_histogramDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_adaptDescriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_adaptDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_postprocessDescriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_postprocessDescriptorSet = VK_NULL_HANDLE;

    float m_adaptedLuminance = 1.0f;
    float m_currentExposure = 1.0f;
    float m_targetLuminance = 1.0f;
    float m_lastFrameTime = 0.0f;

    // Terrain geometry
    std::unique_ptr<rhi::VulkanBuffer> m_vertexBuffer;
    std::unique_ptr<rhi::VulkanBuffer> m_indexBuffer;
    uint32_t m_indexCount = 0;
    float m_minElevation = 3651.0f;
    float m_maxElevation = 8753.0f;

    // Multi-texture PBR & Satellite resources
    void initTexturesAndDescriptors();
    std::vector<std::unique_ptr<rhi::VulkanTexture>> m_textures;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    // Frames in flight synchronization
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
    uint32_t m_lastPresentedImage = 0;
};

} // namespace whiteout::renderer
