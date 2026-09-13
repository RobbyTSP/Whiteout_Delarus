#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace whiteout::core {
    class Window;
}

namespace whiteout::rhi {

class VulkanContext;

class VulkanSwapchain {
public:
    VulkanSwapchain(const VulkanContext& context, const core::Window& window);
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    void recreate(const core::Window& window);

    VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
    VkResult present(VkSemaphore renderCompleteSemaphore, uint32_t imageIndex);

    [[nodiscard]] VkSwapchainKHR getHandle() const { return m_swapchain; }
    [[nodiscard]] VkFormat getImageFormat() const { return m_imageFormat; }
    [[nodiscard]] VkFormat getDepthFormat() const { return m_depthFormat; }
    [[nodiscard]] VkExtent2D getExtent() const { return m_extent; }
    [[nodiscard]] uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }
    [[nodiscard]] VkImage getImage(uint32_t index) const { return m_images[index]; }
    [[nodiscard]] VkImage getDepthImage() const { return m_depthImage; }
    [[nodiscard]] VkImageView getImageView(uint32_t index) const { return m_imageViews[index]; }
    [[nodiscard]] VkImageView getDepthImageView() const { return m_depthImageView; }

private:
    void createSwapchain(const core::Window& window);
    void createImageViews();
    void createDepthResources();
    void cleanup();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const core::Window& window);
    VkFormat findDepthFormat();

    const VulkanContext& m_context;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat m_imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D m_extent{};

    // Depth buffer
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
};

} // namespace whiteout::rhi
