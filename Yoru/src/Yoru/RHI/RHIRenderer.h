#pragma once

#include "Yoru/RHI/RHICommon.h"
#include "Yoru/RHI/RHIInstance.h"
#include "Yoru/RHI/RHIDevice.h"
#include "Yoru/RHI/RHISwapchain.h"
#include "Yoru/RHI/RHIPipeline.h"
#include "Yoru/RHI/RHICommandList.h"

#include <memory>
#include <utility>

namespace Yoru
{
	class RHIRenderer
	{
	public:
		RHIRenderer() = default;
		virtual ~RHIRenderer() = default;

		virtual bool Init(const RendererDesc& desc) = 0;
		virtual void Shutdown() = 0;

		// nullptr when the frame should be skipped (e.g. swapchain was recreated)
		virtual RHICommandList* BeginFrame() = 0;
		virtual void EndFrame() = 0;
		virtual void WaitIdle() = 0;

		// Pipelines are built against the renderer's color/depth targets
		virtual std::unique_ptr<RHIPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
		virtual std::unique_ptr<RHIPipeline> CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
		virtual std::pair<uint32_t, uint32_t> GetRenderExtent() const = 0;
		RHIDevice& GetDevice() { return *m_Device; }

		// Buffers the CPU writes every frame need one copy per frame in flight, index them with this
		virtual uint32_t GetFrameIndex() const = 0;
		virtual uint32_t GetFramesInFlight() const = 0;

		// Backend side of ImGui, the context itself is owned by ImGuiLayer
		virtual void InitImGui() = 0;
		virtual void ShutdownImGui() = 0;
		virtual void ImGuiNewFrame() = 0;
		virtual void RenderImGui(RHICommandList& cmd) = 0;

	protected:
		std::unique_ptr<RHIInstance> m_Instance;
		std::unique_ptr<RHIDevice> m_Device;
		std::unique_ptr<RHISwapchain> m_Swapchain;
	};
}
