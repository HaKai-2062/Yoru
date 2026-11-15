#pragma once

#include "Yoru/RHI/RHISwapchain.h"
#include "Platform/Vulkan/VulkanTypes.h"

struct GLFWwindow;
namespace Yoru
{
	class VulkanInstance;
	class VulkanDevice;
	class VulkanSwapchain : public RHISwapchain
	{
	public:
		VulkanSwapchain() = default;
		~VulkanSwapchain() = default;
	
		virtual bool Init(const SwapchainDesc& desc);
		virtual void Shutdown();

		VkSwapchainKHR GetSwapchain() const { return m_Swapchain; };
		VkExtent2D GetExtent() const { return m_SwapchainExtent;  };
		VkFormat GetFormat() const { return  m_SwapchainFormat; };
		const std::vector<VkImage>& GetImages() const { return m_SwapchainImages; }
		const std::vector<VkImageView>& GetImageViews() const { return m_SwapchainImageViews; }
		bool Recreate();
		void SetContext(const VulkanDeviceContext& context) { m_Context = context; };

		static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* const window);

	private:
		bool CreateSwapchain();
		void DestroySwapchain();
		bool CreateImageViews();
		std::optional<VkImageView> CreateImageView(VkDevice device, VkImage image, 
			VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	private:
		static inline const char* m_ClassName = "VulkanSwapchain";
		VulkanDeviceContext m_Context = {};

		bool m_HasInitialized = false;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		bool m_VSync = false;

		std::vector<VkImage> m_SwapchainImages = {};
		std::vector<VkImageView> m_SwapchainImageViews = {};
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		VkFormat m_SwapchainFormat = VkFormat::VK_FORMAT_UNDEFINED;
		VkExtent2D m_SwapchainExtent = {};
	};
}
