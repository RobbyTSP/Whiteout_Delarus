#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>

namespace whiteout::rhi {

class VulkanContext;

class VulkanTexture {
public:
    VulkanTexture(const VulkanContext& context,
                  const std::string& filepath,
                  bool isSrgb = true,
                  bool clampToEdge = false,
                  bool forceGrayscale = false);

    // Fallback constructor for solid 1x1 pixel textures
    VulkanTexture(const VulkanContext& context,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    // Constructor for raw float / single-channel R32_SFLOAT textures (e.g. DEM heightmaps)
    VulkanTexture(const VulkanContext& context,
                  const float* floatData,
                  uint32_t width,
                  uint32_t height,
                  bool clampToEdge = true);

    ~VulkanTexture();

    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;

    [[nodiscard]] VkImage getImage() const { return m_image; }
    [[nodiscard]] VkImageView getImageView() const { return m_imageView; }
    [[nodiscard]] VkSampler getSampler() const { return m_sampler; }
    [[nodiscard]] VkExtent2D getExtent() const { return m_extent; }

    [[nodiscard]] VkDescriptorImageInfo getDescriptorInfo() const {
        VkDescriptorImageInfo info{};
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = m_imageView;
        info.sampler = m_sampler;
        return info;
    }

private:
    void createTextureImage(const void* pixelData, uint32_t width, uint32_t height, bool isSrgb, uint32_t channels = 4);
    void createFloatTextureImage(const float* floatData, uint32_t width, uint32_t height);
    void generateMipmaps(VkFormat imageFormat, int32_t texWidth, int32_t texHeight);
    void createImageView(bool isSrgb);
    void createTextureSampler(bool clampToEdge);

    const VulkanContext& m_context;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkExtent2D m_extent{0, 0};
    uint32_t m_mipLevels = 1;
    VkFormat m_format = VK_FORMAT_R8G8B8A8_UNORM;
};

} // namespace whiteout::rhi
