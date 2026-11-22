#pragma once

#include "Yoru/RHI/RHICommon.h"

namespace Yoru
{
	class RHIBuffer;
	class RHIPipeline;

	// Per frame view over the frame's command buffer.
	// Handed out by RHIRenderer::BeginFrame and invalid after EndFrame, never store it.
	class RHICommandList
	{
	public:
		RHICommandList() = default;
		virtual ~RHICommandList() = default;
		RHICommandList(const RHICommandList&) = delete;
		RHICommandList& operator=(const RHICommandList&) = delete;

		// Also sets viewport/scissor to the full target
		virtual void BeginRendering(const RenderingDesc& desc) = 0;
		virtual void EndRendering() = 0;

		virtual void BindPipeline(const RHIPipeline& pipeline) = 0;
		virtual void SetViewport(const Viewport& viewport) = 0;
		virtual void SetScissor(const Rect2D& scissor) = 0;
		// Uses the bound pipeline's layout, bind first
		virtual void PushConstants(const void* data, uint32_t size, uint32_t offset = 0) = 0;

		template<typename T>
		void PushConstants(const T& data) { PushConstants(&data, sizeof(T)); }

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1,
			uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
		virtual void BindIndexBuffer(const RHIBuffer& buffer, IndexType type, uint64_t offset = 0) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
			uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) = 0;
		// Reads DrawIndirectArgs from the buffer, which needs BufferUsage::Indirect
		virtual void DrawIndirect(const RHIBuffer& argsBuffer, uint64_t offset = 0, uint32_t drawCount = 1) = 0;

		// Compute and transfer commands are only valid outside BeginRendering/EndRendering
		virtual void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) = 0;
		virtual void DispatchIndirect(const RHIBuffer& argsBuffer, uint64_t offset = 0) = 0;
		// Offset and size must be multiples of 4, size 0 fills to the end of the buffer
		virtual void FillBuffer(const RHIBuffer& buffer, uint32_t value, uint64_t offset = 0, uint64_t size = 0) = 0;

		// Makes writes from one usage visible to the next, e.g. ComputeWrite -> VertexShaderRead
		virtual void BufferBarrier(const RHIBuffer& buffer, ResourceUsage from, ResourceUsage to) = 0;
		// Same for every buffer at once
		virtual void GlobalBarrier(ResourceUsage from, ResourceUsage to) = 0;
	};
}
