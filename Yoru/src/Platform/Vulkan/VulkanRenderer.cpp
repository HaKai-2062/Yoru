#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanInstance.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanInitializers.h"

#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <unordered_set>

namespace Yoru
{
	static void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect,
		VkImageLayout oldLayout, VkImageLayout newLayout,
		VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
		VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess);

	// ImGui's backend still creates the secondary viewport swapchains, but we render and present them
	// ourselves so render complete semaphores are per swapchain image (same scheme as the main window)
	struct ImGuiViewportContext
	{
		VulkanDevice* Device = nullptr;
		VkFormat ColorFormat = VK_FORMAT_UNDEFINED;
		std::unordered_set<ImGuiID> AcquiredViewports;
	};
	static ImGuiViewportContext s_ImGuiViewports;
	static void RenderImGuiViewport(ImGuiViewport* viewport, void*);
	static void PresentImGuiViewport(ImGuiViewport* viewport, void*);

	bool VulkanRenderer::Init(const RendererDesc& desc)
	{
		Log::Write(LogLevel::INFO, "{}: Init", m_ClassName);

		m_Instance = std::unique_ptr<RHIInstance>(CreateInstance(desc.InstDesc));
		if (!m_Instance)
			return false;
		m_VulkanInstance = static_cast<VulkanInstance*>(m_Instance.get());

		m_Device = std::unique_ptr<RHIDevice>(m_Instance->CreateDevice(desc.DeviceDesc));
		if (!m_Device)
			return false;
		m_VulkanDevice = static_cast<VulkanDevice*>(m_Device.get());

		m_Swapchain = std::unique_ptr<RHISwapchain>(m_Device->CreateSwapchain(desc.SwapchainDesc));
		if (!m_Swapchain)
			return false;
		m_VulkanSwapchain = static_cast<VulkanSwapchain*>(m_Swapchain.get());
		m_ColorFormat = m_VulkanSwapchain->GetFormat();

		if (!CreateDepthImage())
			return false;
		if (!CreateFrameData())
			return false;
		if (!CreateRenderSemaphores())
			return false;

		m_HasInitialized = true;
		return true;
	}

	void VulkanRenderer::Shutdown()
	{
		WaitIdle();
		ShutdownImGui();

		if (m_VulkanDevice)
		{
			DestroyRenderSemaphores();
			DestroyFrameData();
			DestroyDepthImage();
		}

		if (m_Swapchain)
			m_Swapchain->Shutdown();
		if (m_Device)
			m_Device->Shutdown();
		if (m_Instance)
			m_Instance->Shutdown();

		m_HasInitialized = false;
		Log::Write(LogLevel::INFO, "{}: Shutdown", m_ClassName);
	}

	RHICommandList* VulkanRenderer::BeginFrame()
	{
		if (!m_HasInitialized)
			return nullptr;

		VkDevice device = m_VulkanDevice->GetDevice();
		FrameData& frame = GetCurrentFrame();

		// Wait till the gpu is done with this frame's command buffer
		VK_CHECK(vkWaitForFences(device, 1, &frame.RenderFence, VK_TRUE, UINT64_MAX));

		VkResult result = vkAcquireNextImageKHR(device, m_VulkanSwapchain->GetSwapchain(), UINT64_MAX,
			frame.SwapchainSemaphore, VK_NULL_HANDLE, &m_SwapchainImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapchain();
			return nullptr;
		}
		// VK_SUBOPTIMAL_KHR still acquired an image, it gets recreated after present
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to acquire swapchain image {}", m_ClassName, string_VkResult(result));
			return nullptr;
		}

		VK_CHECK(vkResetFences(device, 1, &frame.RenderFence));
		VK_CHECK(vkResetCommandBuffer(frame.CommandBuffer, 0));

		VkCommandBuffer cmd = frame.CommandBuffer;
		VkCommandBufferBeginInfo beginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

		TransitionImage(cmd, m_VulkanSwapchain->GetImages()[m_SwapchainImageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

		// Depth is shared between frames in flight, so wait on the previous frame's depth writes
		constexpr VkPipelineStageFlags2 depthStages =
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		TransitionImage(cmd, m_DepthImage, VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			depthStages, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			depthStages, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

		VulkanRenderTarget target{};
		target.ColorView = m_VulkanSwapchain->GetImageViews()[m_SwapchainImageIndex];
		target.DepthView = m_DepthImageView;
		target.Extent = m_VulkanSwapchain->GetExtent();

		VulkanCommandList& commandList = GetCurrentCommandList();
		commandList.Begin(cmd, target);
		return &commandList;
	}

	void VulkanRenderer::EndFrame()
	{
		FrameData& frame = GetCurrentFrame();
		VulkanCommandList& commandList = GetCurrentCommandList();
		VkCommandBuffer cmd = frame.CommandBuffer;

		// The image is presented regardless, make sure it is at least cleared
		if (!commandList.HasRendered())
		{
			commandList.BeginRendering({ .UseDepth = false });
			commandList.EndRendering();
		}
		commandList.End();

		TransitionImage(cmd, m_VulkanSwapchain->GetImages()[m_SwapchainImageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);

		VK_CHECK(vkEndCommandBuffer(cmd));

		VkSemaphore renderSemaphore = m_RenderSemaphores[m_SwapchainImageIndex];

		VkCommandBufferSubmitInfo cmdInfo = VkInit::commandBufferSubmitInfo(cmd);
		// Don't write to the image before the presentation engine has released it
		VkSemaphoreSubmitInfo waitInfo = VkInit::semaphoreSubmitInfo(
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, frame.SwapchainSemaphore);
		VkSemaphoreSubmitInfo signalInfo = VkInit::semaphoreSubmitInfo(
			VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, renderSemaphore);
		VkSubmitInfo2 submit = VkInit::submitInfo(&cmdInfo, &signalInfo, &waitInfo);
		VK_CHECK(vkQueueSubmit2(m_VulkanDevice->GetGraphicsQueue(), 1, &submit, frame.RenderFence));

		VkSwapchainKHR swapchain = m_VulkanSwapchain->GetSwapchain();
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.pImageIndices = &m_SwapchainImageIndex;

		VkResult result = vkQueuePresentKHR(m_VulkanDevice->GetPresentQueue(), &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			RecreateSwapchain();
		else if (result != VK_SUCCESS)
			Log::Write(LogLevel::FATAL, "{}: Failed to present {}", m_ClassName, string_VkResult(result));

		m_FrameIndex++;
	}

	void VulkanRenderer::WaitIdle()
	{
		if (m_VulkanDevice && m_VulkanDevice->GetDevice())
			vkDeviceWaitIdle(m_VulkanDevice->GetDevice());
	}

	std::unique_ptr<RHIPipeline> VulkanRenderer::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{
		auto pipeline = std::make_unique<VulkanPipeline>();
		if (!pipeline->Init(m_VulkanDevice, desc, m_ColorFormat, m_DepthFormat))
			return nullptr;
		return pipeline;
	}

	std::unique_ptr<RHIPipeline> VulkanRenderer::CreateComputePipeline(const ComputePipelineDesc& desc)
	{
		auto pipeline = std::make_unique<VulkanPipeline>();
		if (!pipeline->Init(m_VulkanDevice, desc))
			return nullptr;
		return pipeline;
	}

	std::pair<uint32_t, uint32_t> VulkanRenderer::GetRenderExtent() const
	{
		VkExtent2D extent = m_VulkanSwapchain->GetExtent();
		return { extent.width, extent.height };
	}

	void VulkanRenderer::InitImGui()
	{
		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.ApiVersion = VK_API_VERSION_1_3;
		initInfo.Instance = m_VulkanDevice->GetInstance();
		initInfo.PhysicalDevice = m_VulkanDevice->GetPhysicalDevice();
		initInfo.Device = m_VulkanDevice->GetDevice();
		initInfo.QueueFamily = m_VulkanDevice->GetGraphicsQueueFamily();
		initInfo.Queue = m_VulkanDevice->GetGraphicsQueue();
		initInfo.DescriptorPoolSize = 64;	// backend creates and owns the pool
		initInfo.MinImageCount = 2;
		initInfo.ImageCount = static_cast<uint32_t>(m_VulkanSwapchain->GetImages().size());
		initInfo.UseDynamicRendering = true;
		initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_ColorFormat;
		initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		initInfo.PipelineInfoForViewports.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		initInfo.PipelineInfoForViewports.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		initInfo.PipelineInfoForViewports.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_ColorFormat;
		initInfo.PipelineInfoForViewports.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

		initInfo.CheckVkResultFn = [](VkResult result)
			{
				if (result != VK_SUCCESS)
					Log::Write(LogLevel::ERROR, "ImGui Vulkan backend: {}", string_VkResult(result));
			};
		m_ImGuiInitialized = ImGui_ImplVulkan_Init(&initInfo);
		if (!m_ImGuiInitialized)
		{
			Log::Write(LogLevel::ERROR, "{}: Failed to initialize ImGui Vulkan backend", m_ClassName);
			return;
		}

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			s_ImGuiViewports.Device = m_VulkanDevice;
			s_ImGuiViewports.ColorFormat = m_ColorFormat;
			ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
			platformIO.Renderer_RenderWindow = RenderImGuiViewport;
			platformIO.Renderer_SwapBuffers = PresentImGuiViewport;
		}
	}

	void VulkanRenderer::ShutdownImGui()
	{
		if (!m_ImGuiInitialized)
			return;

		WaitIdle();
		ImGui_ImplVulkan_Shutdown();
		m_ImGuiInitialized = false;
	}

	void VulkanRenderer::ImGuiNewFrame()
	{
		if (m_ImGuiInitialized)
			ImGui_ImplVulkan_NewFrame();
	}

	void VulkanRenderer::RenderImGui(RHICommandList& cmd)
	{
		ImDrawData* drawData = ImGui::GetDrawData();
		if (!m_ImGuiInitialized || !drawData)
			return;

		auto& commandList = static_cast<VulkanCommandList&>(cmd);
		// Draw on top of whatever the layers rendered
		commandList.BeginRendering({ .ClearColorTarget = !commandList.HasRendered(), .UseDepth = false });
		ImGui_ImplVulkan_RenderDrawData(drawData, commandList.GetHandle());
		commandList.EndRendering();
		commandList.InvalidateBoundPipeline();
	}

	VulkanInstance* VulkanRenderer::CreateInstance(const InstanceDesc& desc)
	{
		VulkanInstance* vkInstance = new VulkanInstance;
		if (vkInstance->Init(desc))
		{
			return vkInstance;
		}

		delete vkInstance;
		return nullptr;
	}

	bool VulkanRenderer::CreateFrameData()
	{
		VkDevice device = m_VulkanDevice->GetDevice();
		VkCommandPoolCreateInfo poolInfo = VkInit::commandPoolCreateInfo(
			m_VulkanDevice->GetGraphicsQueueFamily(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		// Signaled so the first BeginFrame doesn't wait forever
		VkFenceCreateInfo fenceInfo = VkInit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
		VkSemaphoreCreateInfo semaphoreInfo = VkInit::semaphoreCreateInfo();

		for (FrameData& frame : m_Frames)
		{
			if (vkCreateCommandPool(device, &poolInfo, nullptr, &frame.CommandPool) != VK_SUCCESS)
			{
				Log::Write(LogLevel::FATAL, "{}: Failed to create command pool", m_ClassName);
				return false;
			}

			VkCommandBufferAllocateInfo allocInfo = VkInit::commandBufferAllocateInfo(frame.CommandPool);
			if (vkAllocateCommandBuffers(device, &allocInfo, &frame.CommandBuffer) != VK_SUCCESS)
			{
				Log::Write(LogLevel::FATAL, "{}: Failed to allocate command buffer", m_ClassName);
				return false;
			}

			if (vkCreateFence(device, &fenceInfo, nullptr, &frame.RenderFence) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.SwapchainSemaphore) != VK_SUCCESS)
			{
				Log::Write(LogLevel::FATAL, "{}: Failed to create frame sync objects", m_ClassName);
				return false;
			}
		}

		return true;
	}

	void VulkanRenderer::DestroyFrameData()
	{
		VkDevice device = m_VulkanDevice->GetDevice();
		if (!device)
			return;

		for (FrameData& frame : m_Frames)
		{
			frame.FrameDeletionQueue.Flush();
			// Destroying the pool frees its command buffers
			if (frame.CommandPool)
				vkDestroyCommandPool(device, frame.CommandPool, nullptr);
			if (frame.RenderFence)
				vkDestroyFence(device, frame.RenderFence, nullptr);
			if (frame.SwapchainSemaphore)
				vkDestroySemaphore(device, frame.SwapchainSemaphore, nullptr);
			frame.CommandPool = VK_NULL_HANDLE;
			frame.CommandBuffer = VK_NULL_HANDLE;
			frame.RenderFence = VK_NULL_HANDLE;
			frame.SwapchainSemaphore = VK_NULL_HANDLE;
		}
	}

	bool VulkanRenderer::CreateRenderSemaphores()
	{
		VkDevice device = m_VulkanDevice->GetDevice();
		VkSemaphoreCreateInfo semaphoreInfo = VkInit::semaphoreCreateInfo();

		m_RenderSemaphores.resize(m_VulkanSwapchain->GetImages().size(), VK_NULL_HANDLE);
		for (VkSemaphore& semaphore : m_RenderSemaphores)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
			{
				Log::Write(LogLevel::FATAL, "{}: Failed to create render semaphore", m_ClassName);
				return false;
			}
		}

		return true;
	}

	void VulkanRenderer::DestroyRenderSemaphores()
	{
		VkDevice device = m_VulkanDevice->GetDevice();
		if (!device)
			return;

		for (VkSemaphore semaphore : m_RenderSemaphores)
		{
			if (semaphore)
				vkDestroySemaphore(device, semaphore, nullptr);
		}
		m_RenderSemaphores.clear();
	}

	bool VulkanRenderer::CreateDepthImage()
	{
		VkExtent2D extent = m_VulkanSwapchain->GetExtent();
		VkImageCreateInfo imageInfo = VkInit::imageCreateInfo(m_DepthFormat,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, { extent.width, extent.height, 1 });

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

		if (vmaCreateImage(m_VulkanDevice->GetAllocator(), &imageInfo, &allocInfo,
			&m_DepthImage, &m_DepthAllocation, nullptr) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create depth image", m_ClassName);
			return false;
		}

		VkImageViewCreateInfo viewInfo = VkInit::imageviewCreateInfo(m_DepthFormat, m_DepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
		if (vkCreateImageView(m_VulkanDevice->GetDevice(), &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create depth image view", m_ClassName);
			return false;
		}

		return true;
	}

	void VulkanRenderer::DestroyDepthImage()
	{
		if (m_DepthImageView)
			vkDestroyImageView(m_VulkanDevice->GetDevice(), m_DepthImageView, nullptr);
		if (m_DepthImage)
			vmaDestroyImage(m_VulkanDevice->GetAllocator(), m_DepthImage, m_DepthAllocation);

		m_DepthImageView = VK_NULL_HANDLE;
		m_DepthImage = VK_NULL_HANDLE;
		m_DepthAllocation = VK_NULL_HANDLE;
	}

	bool VulkanRenderer::RecreateSwapchain()
	{
		// This ensures that swapchain is not being
		//	used before being recreated
		WaitIdle();

		if (!m_VulkanSwapchain->Recreate())
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to recreate swapchain", m_ClassName);
			m_HasInitialized = false;
			return false;
		}

		DestroyDepthImage();
		if (!CreateDepthImage())
		{
			m_HasInitialized = false;
			return false;
		}

		// Image count can change between swapchains
		DestroyRenderSemaphores();
		return CreateRenderSemaphores();
	}

	static void RebuildImGuiViewport(ImGuiViewport* viewport, ImGui_ImplVulkanH_Window* wd)
	{
		VulkanDevice* device = s_ImGuiViewports.Device;
		// Waits for the device to go idle before recreating
		ImGui_ImplVulkanH_CreateOrResizeWindow(device->GetInstance(), device->GetPhysicalDevice(), device->GetDevice(), wd,
			device->GetGraphicsQueueFamily(), nullptr, static_cast<int>(viewport->Size.x), static_cast<int>(viewport->Size.y), 2, 0);
		wd->SemaphoreIndex = 0;
	}

	static void RenderImGuiViewport(ImGuiViewport* viewport, void*)
	{
		ImGui_ImplVulkanH_Window* wd = ImGui_ImplVulkanH_GetWindowDataFromViewport(viewport);
		if (!wd || wd->Swapchain == VK_NULL_HANDLE)
			return;

		// ImGui draws with the main window's pipeline, built for the main swapchain format
		if (wd->SurfaceFormat.format != s_ImGuiViewports.ColorFormat)
		{
			Log::Write(LogLevel::ERROR, "VulkanRenderer: ImGui viewport format {} differs from the main swapchain's {}",
				string_VkFormat(wd->SurfaceFormat.format), string_VkFormat(s_ImGuiViewports.ColorFormat));
			return;
		}

		VkDevice device = s_ImGuiViewports.Device->GetDevice();
		VkSemaphore acquireSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
		VkResult result = vkAcquireNextImageKHR(device, wd->Swapchain, UINT64_MAX, acquireSemaphore, VK_NULL_HANDLE, &wd->FrameIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RebuildImGuiViewport(viewport, wd);
			return;
		}
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			Log::Write(LogLevel::ERROR, "VulkanRenderer: Failed to acquire ImGui viewport image {}", string_VkResult(result));
			return;
		}

		// Frames are per swapchain image, so this fence covers the last use of this image's command buffer
		ImGui_ImplVulkanH_Frame& frame = wd->Frames[wd->FrameIndex];
		VK_CHECK(vkWaitForFences(device, 1, &frame.Fence, VK_TRUE, UINT64_MAX));
		VK_CHECK(vkResetFences(device, 1, &frame.Fence));
		VK_CHECK(vkResetCommandPool(device, frame.CommandPool, 0));

		VkCommandBuffer cmd = frame.CommandBuffer;
		VkCommandBufferBeginInfo beginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

		TransitionImage(cmd, frame.Backbuffer, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

		VkClearValue clearValue{};
		clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		VkRenderingAttachmentInfo colorAttachment = VkInit::attachmentInfo(frame.BackbufferView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkRenderingInfo renderInfo = VkInit::renderingInfo({ static_cast<uint32_t>(wd->Width), static_cast<uint32_t>(wd->Height) }, &colorAttachment, nullptr);
		vkCmdBeginRendering(cmd, &renderInfo);
		ImGui_ImplVulkan_RenderDrawData(viewport->DrawData, cmd);
		vkCmdEndRendering(cmd);

		TransitionImage(cmd, frame.Backbuffer, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
		VK_CHECK(vkEndCommandBuffer(cmd));

		// Render complete semaphore is picked by image index, not rotated per frame
		VkCommandBufferSubmitInfo cmdInfo = VkInit::commandBufferSubmitInfo(cmd);
		VkSemaphoreSubmitInfo waitInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, acquireSemaphore);
		VkSemaphoreSubmitInfo signalInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
			wd->FrameSemaphores[wd->FrameIndex].RenderCompleteSemaphore);
		VkSubmitInfo2 submit = VkInit::submitInfo(&cmdInfo, &signalInfo, &waitInfo);
		VK_CHECK(vkQueueSubmit2(s_ImGuiViewports.Device->GetGraphicsQueue(), 1, &submit, frame.Fence));

		s_ImGuiViewports.AcquiredViewports.insert(viewport->ID);
	}

	static void PresentImGuiViewport(ImGuiViewport* viewport, void*)
	{
		// Nothing was acquired this frame (e.g. swapchain was being rebuilt)
		if (s_ImGuiViewports.AcquiredViewports.erase(viewport->ID) == 0)
			return;

		ImGui_ImplVulkanH_Window* wd = ImGui_ImplVulkanH_GetWindowDataFromViewport(viewport);
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &wd->FrameSemaphores[wd->FrameIndex].RenderCompleteSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &wd->Swapchain;
		presentInfo.pImageIndices = &wd->FrameIndex;

		VkResult result = vkQueuePresentKHR(s_ImGuiViewports.Device->GetGraphicsQueue(), &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			RebuildImGuiViewport(viewport, wd);
			return;
		}
		if (result != VK_SUCCESS)
			Log::Write(LogLevel::ERROR, "VulkanRenderer: Failed to present ImGui viewport {}", string_VkResult(result));

		// Acquire semaphores rotate since the image index isn't known before acquiring
		wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
	}

	static void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect,
		VkImageLayout oldLayout, VkImageLayout newLayout,
		VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
		VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
	{
		VkImageMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.srcStageMask = srcStage;
		barrier.srcAccessMask = srcAccess;
		barrier.dstStageMask = dstStage;
		barrier.dstAccessMask = dstAccess;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange = VkInit::imageSubresourceRange(aspect);

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &barrier;

		vkCmdPipelineBarrier2(cmd, &depInfo);
	}
}
