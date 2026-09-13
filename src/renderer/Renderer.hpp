#pragma once

#include "../core/Window.hpp"
#include "../core/Camera.hpp"
#include "../rhi/VulkanContext.hpp"
#include "../rhi/VulkanSwapchain.hpp"
#include "../rhi/VulkanPipeline.hpp"
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

private:
    void initSyncObjects();
    void createCommandBuffers();
    void generateTerrainMesh(uint32_t gridResolution = 256);
    void loadEverestDem(const std::string& manifestPath, const std::string& demBinPath, uint32_t sampleStep = 4);

    core::Window& m_window;
    std::unique_ptr<rhi::VulkanContext> m_context;
    std::unique_ptr<rhi::VulkanSwapchain> m_swapchain;
    std::unique_ptr<rhi::VulkanPipeline> m_pipeline;

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
