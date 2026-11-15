#pragma once

#include "Yoru/RHI/RHIShader.h"
#include "Platform/Vulkan/VulkanTypes.h"

namespace Yoru
{
	class VulkanDevice;

	class VulkanShader : public RHIShader
	{
	public:
		VulkanShader(VulkanDevice* device, const ShaderDesc& desc);
		~VulkanShader();
		
		std::vector<char> ReadBinaryFile(const std::string& path);
		ShaderStage GetStage() const override { return m_Stage; };
		VkShaderModule GetModule() const { return m_Module; };

	private:
		static inline const char* m_ClassName = "VulkanShader";
		VulkanDevice* m_Device;
		VkShaderModule m_Module = VK_NULL_HANDLE;
		ShaderStage m_Stage;
		std::string m_EntryPoint;
	};
}
