#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace whiteout::rhi {

class VulkanContext;

class VulkanBuffer {
public:
    VulkanBuffer(const VulkanContext& context,
                 VkDeviceSize size,
                 VkBufferUsageFlags usage,
                 VkMemoryPropertyFlags properties);
    ~VulkanBuffer();

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    void upload(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    [[nodiscard]] VkBuffer getHandle() const { return m_buffer; }
    [[nodiscard]] VkDeviceMemory getMemory() const { return m_memory; }
    [[nodiscard]] VkDeviceSize getSize() const { return m_size; }

    static std::unique_ptr<VulkanBuffer> createDeviceLocalBuffer(
        const VulkanContext& context,
        const void* data,
        VkDeviceSize size,
        VkBufferUsageFlags usage
    );

private:
    const VulkanContext& m_context;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
};

} // namespace whiteout::rhi
