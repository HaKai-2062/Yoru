#pragma once

#include "Yoru/RHI/RHICommon.h"
#include "Yoru/RHI/RHIRenderer.h"
#include "Platform/Vulkan/VulkanTypes.h"
#include "Platform/Vulkan/VulkanCommandList.h"

namespace Yoru
{
	class VulkanInstance;
	class VulkanDevice;
	class VulkanSwapchain;

	class VulkanRenderer : public RHIRenderer
	{
	public:
		VulkanRenderer() = default;
		~VulkanRenderer() = default;

		virtual bool Init(const RendererDesc& desc) override;
		virtual void Shutdown() override;

		virtual RHICommandList* BeginFrame() override;
		virtual void EndFrame() override;
		virtual void WaitIdle() override;

		virtual std::unique_ptr<RHIPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
		virtual std::pair<uint32_t, uint32_t> GetRenderExtent() const override;

		virtual void InitImGui() override;
		virtual void ShutdownImGui() override;
		virtual void ImGuiNewFrame() override;
		virtual void RenderImGui(RHICommandList& cmd) override;

		VulkanInstance* CreateInstance(const InstanceDesc& desc);

	private:
		bool CreateFrameData();
		void DestroyFrameData();
		bool CreateRenderSemaphores();
		void DestroyRenderSemaphores();
		bool CreateDepthImage();
		void DestroyDepthImage();
		bool RecreateSwapchain();

		FrameData& GetCurrentFrame() { return m_Frames[m_FrameIndex % MAX_FRAMES_IN_FLIGHT]; }
		VulkanCommandList& GetCurrentCommandList() { return m_CommandLists[m_FrameIndex % MAX_FRAMES_IN_FLIGHT]; }

	private:
		static inline const char* m_ClassName = "VulkanRenderer";
		bool m_HasInitialized = false;
		bool m_ImGuiInitialized = false;

		VulkanInstance* m_VulkanInstance = nullptr;
		VulkanDevice* m_VulkanDevice = nullptr;
		VulkanSwapchain* m_VulkanSwapchain = nullptr;

		FrameData m_Frames[MAX_FRAMES_IN_FLIGHT] = {};
		VulkanCommandList m_CommandLists[MAX_FRAMES_IN_FLIGHT];
		// One per swapchain image, present may still be reading it after the frame fence signals
		std::vector<VkSemaphore> m_RenderSemaphores = {};
		uint64_t m_FrameIndex = 0;
		uint32_t m_SwapchainImageIndex = 0;

		// Kept as a member since ImGui holds a pointer to it
		VkFormat m_ColorFormat = VK_FORMAT_UNDEFINED;
		VkFormat m_DepthFormat = VK_FORMAT_D32_SFLOAT;
		VkImage m_DepthImage = VK_NULL_HANDLE;
		VkImageView m_DepthImageView = VK_NULL_HANDLE;
		VmaAllocation m_DepthAllocation = VK_NULL_HANDLE;
	};
}
