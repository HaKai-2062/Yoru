#include "Platform/Vulkan/VulkanCommandList.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanBuffer.h"

#include <Volk/volk.h>

namespace Yoru
{
	struct VulkanSyncScope
	{
		VkPipelineStageFlags2 Stages = VK_PIPELINE_STAGE_2_NONE;
		VkAccessFlags2 Access = VK_ACCESS_2_NONE;
	};

	static VulkanSyncScope ToVulkanSync(ResourceUsage usage)
	{
		VulkanSyncScope scope{};
		auto add = [&](ResourceUsage flag, VkPipelineStageFlags2 stage, VkAccessFlags2 access)
			{
				if (HasFlag(usage, flag))
				{
					scope.Stages |= stage;
					scope.Access |= access;
				}
			};

		add(ResourceUsage::TransferRead, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
		add(ResourceUsage::TransferWrite, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
		add(ResourceUsage::ComputeRead, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
		add(ResourceUsage::ComputeWrite, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
		add(ResourceUsage::VertexShaderRead, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
		add(ResourceUsage::FragmentShaderRead, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
		add(ResourceUsage::IndexRead, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT);
		add(ResourceUsage::IndirectRead, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
		add(ResourceUsage::HostRead, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
		return scope;
	}

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
		if (m_InsideRendering && vkPipeline.GetType() == PipelineType::Graphics &&
			vkPipeline.UsesDepthTarget() != m_PassUsesDepth)
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

	void VulkanCommandList::DrawIndirect(const RHIBuffer& argsBuffer, uint64_t offset, uint32_t drawCount)
	{
		if (!ValidateDraw("DrawIndirect"))
			return;
		const auto& vkBuffer = static_cast<const VulkanBuffer&>(argsBuffer);
		vkCmdDrawIndirect(m_Cmd, vkBuffer.GetBuffer(), offset, drawCount, sizeof(DrawIndirectArgs));
	}

	void VulkanCommandList::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
	{
		if (!ValidateDispatch("Dispatch"))
			return;
		vkCmdDispatch(m_Cmd, groupsX, groupsY, groupsZ);
	}

	void VulkanCommandList::DispatchIndirect(const RHIBuffer& argsBuffer, uint64_t offset)
	{
		if (!ValidateDispatch("DispatchIndirect"))
			return;
		const auto& vkBuffer = static_cast<const VulkanBuffer&>(argsBuffer);
		vkCmdDispatchIndirect(m_Cmd, vkBuffer.GetBuffer(), offset);
	}

	void VulkanCommandList::FillBuffer(const RHIBuffer& buffer, uint32_t value, uint64_t offset, uint64_t size)
	{
		if (!ValidateOutsideRendering("FillBuffer"))
			return;
		const auto& vkBuffer = static_cast<const VulkanBuffer&>(buffer);
		vkCmdFillBuffer(m_Cmd, vkBuffer.GetBuffer(), offset, size == 0 ? VK_WHOLE_SIZE : size, value);
	}

	void VulkanCommandList::BufferBarrier(const RHIBuffer& buffer, ResourceUsage from, ResourceUsage to)
	{
		if (!ValidateOutsideRendering("BufferBarrier"))
			return;

		const VulkanSyncScope src = ToVulkanSync(from);
		const VulkanSyncScope dst = ToVulkanSync(to);

		VkBufferMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		barrier.srcStageMask = src.Stages;
		barrier.srcAccessMask = src.Access;
		barrier.dstStageMask = dst.Stages;
		barrier.dstAccessMask = dst.Access;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = static_cast<const VulkanBuffer&>(buffer).GetBuffer();
		barrier.size = VK_WHOLE_SIZE;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.bufferMemoryBarrierCount = 1;
		depInfo.pBufferMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(m_Cmd, &depInfo);
	}

	void VulkanCommandList::GlobalBarrier(ResourceUsage from, ResourceUsage to)
	{
		if (!ValidateOutsideRendering("GlobalBarrier"))
			return;

		const VulkanSyncScope src = ToVulkanSync(from);
		const VulkanSyncScope dst = ToVulkanSync(to);

		VkMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		barrier.srcStageMask = src.Stages;
		barrier.srcAccessMask = src.Access;
		barrier.dstStageMask = dst.Stages;
		barrier.dstAccessMask = dst.Access;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.memoryBarrierCount = 1;
		depInfo.pMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(m_Cmd, &depInfo);
	}

	bool VulkanCommandList::ValidateDispatch(const char* function) const
	{
		if (!ValidateOutsideRendering(function))
			return false;
		if (!m_BoundPipeline || m_BoundPipeline->GetType() != PipelineType::Compute)
		{
			Log::Write(LogLevel::ERROR, "{}: {} needs a bound compute pipeline", m_ClassName, function);
			return false;
		}
		return true;
	}

	bool VulkanCommandList::ValidateOutsideRendering(const char* function) const
	{
		if (m_InsideRendering)
		{
			Log::Write(LogLevel::ERROR, "{}: {} isn't allowed inside BeginRendering/EndRendering", m_ClassName, function);
			return false;
		}
		return true;
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
