#pragma once

#include "Yoru/RHI/RHICommandList.h"
#include "Platform/Vulkan/VulkanTypes.h"

namespace Yoru
{
	class VulkanPipeline;

	// What the renderer targets this frame
	struct VulkanRenderTarget
	{
		VkImageView ColorView = VK_NULL_HANDLE;
		VkImageView DepthView = VK_NULL_HANDLE;
		VkExtent2D Extent = {};
	};

	class VulkanCommandList : public RHICommandList
	{
	public:
		VulkanCommandList() = default;
		~VulkanCommandList() = default;

		// Called by VulkanRenderer only
		void Begin(VkCommandBuffer cmd, const VulkanRenderTarget& target);
		void End();
		VkCommandBuffer GetHandle() const { return m_Cmd; }
		// Whether anything rendered to the color target this frame
		bool HasRendered() const { return m_HasRendered; }
		// For code that records directly into the handle (e.g. ImGui)
		void InvalidateBoundPipeline() { m_BoundPipeline = nullptr; }

		virtual void BeginRendering(const RenderingDesc& desc) override;
		virtual void EndRendering() override;

		virtual void BindPipeline(const RHIPipeline& pipeline) override;
		virtual void SetViewport(const Viewport& viewport) override;
		virtual void SetScissor(const Rect2D& scissor) override;
		virtual void PushConstants(const void* data, uint32_t size, uint32_t offset = 0) override;
		using RHICommandList::PushConstants;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1,
			uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
		virtual void BindIndexBuffer(const RHIBuffer& buffer, IndexType type, uint64_t offset = 0) override;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
			uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) override;
		virtual void DrawIndirect(const RHIBuffer& argsBuffer, uint64_t offset = 0, uint32_t drawCount = 1) override;

		virtual void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) override;
		virtual void DispatchIndirect(const RHIBuffer& argsBuffer, uint64_t offset = 0) override;
		virtual void FillBuffer(const RHIBuffer& buffer, uint32_t value, uint64_t offset = 0, uint64_t size = 0) override;

		virtual void BufferBarrier(const RHIBuffer& buffer, ResourceUsage from, ResourceUsage to) override;
		virtual void GlobalBarrier(ResourceUsage from, ResourceUsage to) override;

	private:
		bool ValidateDraw(const char* function) const;
		bool ValidateDispatch(const char* function) const;
		bool ValidateOutsideRendering(const char* function) const;

	private:
		static inline const char* m_ClassName = "VulkanCommandList";

		VkCommandBuffer m_Cmd = VK_NULL_HANDLE;
		VulkanRenderTarget m_Target = {};
		const VulkanPipeline* m_BoundPipeline = nullptr;
		bool m_InsideRendering = false;
		bool m_PassUsesDepth = false;
		bool m_HasRendered = false;
	};
}
