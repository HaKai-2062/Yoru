#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanInitializers.h"

#include <Volk/volk.h>

namespace Yoru
{
	static VkPrimitiveTopology ToVulkan(PrimitiveTopology topology)
	{
		switch (topology)
		{
		case PrimitiveTopology::LineList:	return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case PrimitiveTopology::PointList:	return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		default:							return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		}
	}

	static VkCullModeFlags ToVulkan(CullMode cull)
	{
		switch (cull)
		{
		case CullMode::Front:	return VK_CULL_MODE_FRONT_BIT;
		case CullMode::Back:	return VK_CULL_MODE_BACK_BIT;
		default:				return VK_CULL_MODE_NONE;
		}
	}

	VulkanPipeline::~VulkanPipeline()
	{
		if (!m_Device)
			return;

		VkDevice device = m_Device->GetDevice();
		if (m_Pipeline)
			vkDestroyPipeline(device, m_Pipeline, nullptr);
		if (m_Layout)
			vkDestroyPipelineLayout(device, m_Layout, nullptr);
	}

	bool VulkanPipeline::Init(VulkanDevice* device, const GraphicsPipelineDesc& desc,
		VkFormat colorFormat, VkFormat depthFormat)
	{
		m_Device = device;
		VkDevice vkDevice = m_Device->GetDevice();

		if (!desc.VertexShader || !desc.FragmentShader)
		{
			Log::Write(LogLevel::ERROR, "{}: Graphics pipeline needs a vertex and fragment shader", m_ClassName);
			return false;
		}

		m_PushConstantSize = desc.PushConstantSize;
		m_UseDepthTarget = desc.UseDepthTarget;
		m_PushConstantStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkPushConstantRange pushRange{};
		pushRange.stageFlags = m_PushConstantStages;
		pushRange.size = m_PushConstantSize;

		VkPipelineLayoutCreateInfo layoutInfo = VkInit::pipelineLayoutCreateInfo();
		if (m_PushConstantSize > 0)
		{
			layoutInfo.pushConstantRangeCount = 1;
			layoutInfo.pPushConstantRanges = &pushRange;
		}

		if (vkCreatePipelineLayout(vkDevice, &layoutInfo, nullptr, &m_Layout) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create pipeline layout", m_ClassName);
			return false;
		}

		const auto* vertexShader = static_cast<const VulkanShader*>(desc.VertexShader);
		const auto* fragmentShader = static_cast<const VulkanShader*>(desc.FragmentShader);
		VkPipelineShaderStageCreateInfo stages[2] =
		{
			VkInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader->GetModule()),
			VkInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader->GetModule())
		};

		// Vertices are pulled through buffer device addresses
		VkPipelineVertexInputStateCreateInfo vertexInput{};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = ToVulkan(desc.Topology);

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.cullMode = ToVulkan(desc.Cull);
		rasterizer.frontFace = desc.Front == FrontFace::Clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = desc.UseDepthTarget && desc.DepthTest;
		depthStencil.depthWriteEnable = desc.UseDepthTarget && desc.DepthWrite;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencil.maxDepthBounds = 1.0f;

		VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
		dynamicState.pDynamicStates = dynamicStates;

		// Dynamic rendering: attachment formats replace the renderpass
		VkPipelineRenderingCreateInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachmentFormats = &colorFormat;
		renderingInfo.depthAttachmentFormat = desc.UseDepthTarget ? depthFormat : VK_FORMAT_UNDEFINED;

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext = &renderingInfo;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_Layout;

		if (vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create graphics pipeline", m_ClassName);
			return false;
		}

		return true;
	}
}
