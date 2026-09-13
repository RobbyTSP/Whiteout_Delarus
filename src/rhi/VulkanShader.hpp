#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace whiteout::rhi {

class VulkanShader {
public:
    static VkShaderModule createShaderModule(VkDevice device, const std::string& filepath);
    static std::vector<char> readSpirvFile(const std::string& filepath);
};

} // namespace whiteout::rhi
