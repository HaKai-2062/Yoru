#include "Platform/Vulkan/VulkanCommandList.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanBuffer.h"

#include <Volk/volk.h>

namespace Yoru
{
	void VulkanCommandList::Begin(VkCommandBuffer cmd, const VulkanRenderTarget& target)
	{
		m_Cmd = cmd;
		m_Target = target;
		m_BoundPipeline = nullptr;
		m_InsideRendering = false;
		m_PassUsesDepth = false;
		m_HasRendered = false;
	}

	void VulkanCommandList::End()
	{
		if (m_InsideRendering)
		{
			Log::Write(LogLevel::ERROR, "{}: Frame ended inside a rendering pass, closing it", m_ClassName);
			EndRendering();
		}
		m_Cmd = VK_NULL_HANDLE;
	}

	void VulkanCommandList::BeginRendering(const RenderingDesc& desc)
	{
		if (m_InsideRendering)
		{
			Log::Write(LogLevel::ERROR, "{}: BeginRendering called inside another pass", m_ClassName);
			return;
		}

		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = m_Target.ColorView;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = desc.ClearColorTarget ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = { { desc.Clear.R, desc.Clear.G, desc.Clear.B, desc.Clear.A } };

		VkRenderingAttachmentInfo depthAttachment{};
		depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachment.imageView = m_Target.DepthView;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depthAttachment.loadOp = desc.ClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.clearValue.depthStencil.depth = 1.0f;

		VkRenderingInfo renderInfo{};
		renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderInfo.renderArea = { { 0, 0 }, m_Target.Extent };
		renderInfo.layerCount = 1;
		renderInfo.colorAttachmentCount = 1;
		renderInfo.pColorAttachments = &colorAttachment;
		renderInfo.pDepthAttachment = desc.UseDepth ? &depthAttachment : nullptr;

		vkCmdBeginRendering(m_Cmd, &renderInfo);
		m_InsideRendering = true;
		m_PassUsesDepth = desc.UseDepth;
		m_HasRendered = true;

		// Dynamic state doesn't survive across command buffers, set defaults every pass
		SetViewport({ 0.0f, 0.0f, static_cast<float>(m_Target.Extent.width), static_cast<float>(m_Target.Extent.height) });
		SetScissor({ 0, 0, m_Target.Extent.width, m_Target.Extent.height });
	}

	void VulkanCommandList::EndRendering()
	{
		if (!m_InsideRendering)
			return;

		vkCmdEndRendering(m_Cmd);
		m_InsideRendering = false;
	}

	void VulkanCommandList::BindPipeline(const RHIPipeline& pipeline)
	{
		const auto& vkPipeline = static_cast<const VulkanPipeline&>(pipeline);
		if (m_InsideRendering && vkPipeline.UsesDepthTarget() != m_PassUsesDepth)
			Log::Write(LogLevel::ERROR, "{}: Pipeline depth target doesn't match the current pass", m_ClassName);

		vkCmdBindPipeline(m_Cmd, vkPipeline.GetBindPoint(), vkPipeline.GetPipeline());
		m_BoundPipeline = &vkPipeline;
	}

	void VulkanCommandList::SetViewport(const Viewport& viewport)
	{
		VkViewport vkViewport{ viewport.X, viewport.Y, viewport.Width, viewport.Height, viewport.MinDepth, viewport.MaxDepth };
		vkCmdSetViewport(m_Cmd, 0, 1, &vkViewport);
	}

	void VulkanCommandList::SetScissor(const Rect2D& scissor)
	{
		VkRect2D vkScissor{ { scissor.X, scissor.Y }, { scissor.Width, scissor.Height } };
		vkCmdSetScissor(m_Cmd, 0, 1, &vkScissor);
	}

	void VulkanCommandList::PushConstants(const void* data, uint32_t size, uint32_t offset)
	{
		if (!m_BoundPipeline)
		{
			Log::Write(LogLevel::ERROR, "{}: PushConstants needs a bound pipeline", m_ClassName);
			return;
		}
		if (offset + size > m_BoundPipeline->GetPushConstantSize())
		{
			Log::Write(LogLevel::ERROR, "{}: PushConstants of {} bytes exceeds the pipeline's {} bytes",
				m_ClassName, offset + size, m_BoundPipeline->GetPushConstantSize());
			return;
		}

		vkCmdPushConstants(m_Cmd, m_BoundPipeline->GetLayout(), m_BoundPipeline->GetPushConstantStages(), offset, size, data);
	}

	void VulkanCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		if (!ValidateDraw("Draw"))
			return;
		vkCmdDraw(m_Cmd, vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void VulkanCommandList::BindIndexBuffer(const RHIBuffer& buffer, IndexType type, uint64_t offset)
	{
		const auto& vkBuffer = static_cast<const VulkanBuffer&>(buffer);
		vkCmdBindIndexBuffer(m_Cmd, vkBuffer.GetBuffer(), offset,
			type == IndexType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
	}

	void VulkanCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
		uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		if (!ValidateDraw("DrawIndexed"))
			return;
		vkCmdDrawIndexed(m_Cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	bool VulkanCommandList::ValidateDraw(const char* function) const
	{
		if (!m_InsideRendering)
		{
			Log::Write(LogLevel::ERROR, "{}: {} called outside BeginRendering/EndRendering", m_ClassName, function);
			return false;
		}
		if (!m_BoundPipeline || m_BoundPipeline->GetType() != PipelineType::Graphics)
		{
			Log::Write(LogLevel::ERROR, "{}: {} needs a bound graphics pipeline", m_ClassName, function);
			return false;
		}
		return true;
	}
}
