#pragma once

#include "Yoru/RHI/RHIPipeline.h"
#include "Platform/Vulkan/VulkanTypes.h"

namespace Yoru
{
	class VulkanDevice;

	// Graphics pipeline built for dynamic rendering (no VkRenderPass)
	class VulkanPipeline : public RHIPipeline
	{
	public:
		VulkanPipeline() = default;
		~VulkanPipeline();
		VulkanPipeline(const VulkanPipeline&) = delete;
		VulkanPipeline& operator=(const VulkanPipeline&) = delete;

		bool Init(VulkanDevice* device, const GraphicsPipelineDesc& desc,
			VkFormat colorFormat, VkFormat depthFormat);

		virtual PipelineType GetType() const override { return m_Type; }
		virtual uint32_t GetPushConstantSize() const override { return m_PushConstantSize; }

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetLayout() const { return m_Layout; }
		VkPipelineBindPoint GetBindPoint() const { return m_BindPoint; }
		VkShaderStageFlags GetPushConstantStages() const { return m_PushConstantStages; }
		bool UsesDepthTarget() const { return m_UseDepthTarget; }

	private:
		static inline const char* m_ClassName = "VulkanPipeline";
		VulkanDevice* m_Device = nullptr;

		VkPipeline m_Pipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_Layout = VK_NULL_HANDLE;
		PipelineType m_Type = PipelineType::Graphics;
		VkPipelineBindPoint m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		VkShaderStageFlags m_PushConstantStages = 0;
		uint32_t m_PushConstantSize = 0;
		bool m_UseDepthTarget = true;
	};
}
