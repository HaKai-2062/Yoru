#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanDevice.h"

#include <Volk/volk.h>

namespace Yoru
{
    std::vector<char> VulkanShader::ReadBinaryFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file)
        {
            Log::Write(LogLevel::ERROR, "{}: Failed to open shader file {}", m_ClassName, path);
            return {};
        }

        const size_t size = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(size);
        file.seekg(0);
        file.read(buffer.data(), size);

        return buffer;
    }

    VulkanShader::VulkanShader(VulkanDevice* device, const ShaderDesc& desc)
        : m_Device(device), m_Stage(desc.Stage), m_EntryPoint(desc.EntryPoint)
    {
        const auto code = ReadBinaryFile(desc.Path);

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        if (vkCreateShaderModule(m_Device->GetDevice(), &createInfo, 
            nullptr, &m_Module) != VK_SUCCESS)
        {
            Log::Write(LogLevel::FATAL, "{}: Failed to create shader module {}", m_ClassName, desc.Path);
        }
    }

    VulkanShader::~VulkanShader()
    {
        if (m_Module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_Device->GetDevice(), m_Module, nullptr);
        }
    }
}
