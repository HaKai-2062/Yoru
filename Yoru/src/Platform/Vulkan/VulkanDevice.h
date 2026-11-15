#pragma once

#include "Yoru/RHI/RHIDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanTypes.h"

struct GLFWwindow;
namespace Yoru
{
	class VulkanInstance;
	class VulkanSwapchain;
	class VulkanShader;

	class VulkanDevice : public RHIDevice
	{
	public:
		VulkanDevice() = default;
		~VulkanDevice() = default;
		virtual bool Init(const DeviceDesc& desc) override;
		virtual void Shutdown() override;

		VkInstance GetInstance() const { return m_Context.Instance; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetDevice() const { return m_Device; }
		VmaAllocator GetAllocator() const { return m_Allocator; }
		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }
		uint32_t GetGraphicsQueueFamily() const { return m_QueueIndices.GraphicsFamily.value(); }

		void SetVulkanInstance(VulkanInstance* instance) { m_VulkanInstance = instance; }
		const VulkanDeviceContext& GetContext() const { return m_Context; }
		virtual VulkanSwapchain* CreateSwapchain(const SwapchainDesc& desc) override;
		virtual std::unique_ptr<RHIShader> CreateShader(const ShaderDesc& desc) override;
		virtual std::unique_ptr<RHIBuffer> CreateBuffer(const BufferDesc& desc) override;

		// Records and submits on the graphics queue, blocks until finished
		void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, 
			VkSurfaceKHR surface);

	private:
		bool PickPhysicalDevice();
		bool CreateLogicalDevice();
		bool CreateAllocator();
		bool CreateImmediateCommands();

		bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
		bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
		bool CheckDeviceFeatureSupport(VkPhysicalDevice device);
		void PrintPhysicalDeviceInfo(VkPhysicalDevice device);
		VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice);

	private:
		static inline const char* m_ClassName = "VulkanDevice";
		bool m_HasInitialized = false;
		
		VulkanDeviceContext m_Context = {};
		VulkanInstance* m_VulkanInstance = nullptr;
		
		QueueFamilyIndices m_QueueIndices = {};
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_Device = VK_NULL_HANDLE;
		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_PresentQueue = VK_NULL_HANDLE;
		VmaAllocator m_Allocator = VK_NULL_HANDLE;

		VkCommandPool m_ImmediatePool = VK_NULL_HANDLE;
		VkCommandBuffer m_ImmediateCommandBuffer = VK_NULL_HANDLE;
		VkFence m_ImmediateFence = VK_NULL_HANDLE;
		
		// Add this to vulkanrenderer later on
		DeletionQueue m_MainDeletionQueue = {};

		const std::vector<const char*> m_DeviceExtensions =
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};
	};
}
