#include "VulkanBuffer.hpp"
#include "VulkanContext.hpp"
#include <cstring>
#include <stdexcept>

namespace whiteout::rhi {

VulkanBuffer::VulkanBuffer(const VulkanContext& context,
                           VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties)
    : m_context(context), m_size(size) {
    VkDevice device = m_context.getDevice();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, m_buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_context.findMemoryType(memRequirements.memoryTypeBits, properties);

    VkMemoryAllocateFlagsInfo flagsInfo{};
    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        allocInfo.pNext = &flagsInfo;
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, m_buffer, m_memory, 0);
}

VulkanBuffer::~VulkanBuffer() {
    VkDevice device = m_context.getDevice();
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_buffer, nullptr);
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
    }
}

void VulkanBuffer::upload(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    VkDevice device = m_context.getDevice();
    void* mappedData = nullptr;
    vkMapMemory(device, m_memory, offset, size, 0, &mappedData);
    std::memcpy(mappedData, data, static_cast<size_t>(size));
    vkUnmapMemory(device, m_memory);
}

std::unique_ptr<VulkanBuffer> VulkanBuffer::createDeviceLocalBuffer(
    const VulkanContext& context,
    const void* data,
    VkDeviceSize size,
    VkBufferUsageFlags usage
) {
    // 1. Create Host-visible Staging Buffer
    VulkanBuffer stagingBuffer(
        context,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    stagingBuffer.upload(data, size);

    // 2. Create Device-Local GPU Buffer
    auto deviceLocalBuffer = std::make_unique<VulkanBuffer>(
        context,
        size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // 3. Copy via single-time command buffer
    VkCommandBuffer cmd = context.beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, stagingBuffer.getHandle(), deviceLocalBuffer->getHandle(), 1, &copyRegion);
    context.endSingleTimeCommands(cmd);

    return deviceLocalBuffer;
}

} // namespace whiteout::rhi
