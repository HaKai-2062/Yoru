#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanInstance.h"

#include <Volk/volk.h>
#include <GLFW/glfw3.h>

namespace Yoru
{
	bool VulkanSwapchain::Init(const SwapchainDesc& desc)
	{
		m_Width = desc.Width;
		m_Height = desc.Height;
		m_VSync = desc.VSync;

		if (!CreateSwapchain())
			return false;
		if (!CreateImageViews())
			return false;

		Log::Write(LogLevel::INFO, "{}: Init", m_ClassName);

		m_HasInitialized = true;
		return true;
	}

	void VulkanSwapchain::Shutdown()
	{
		DestroySwapchain();
		m_HasInitialized = false;
		m_Context = {};

		Log::Write(LogLevel::INFO, "{}: Shutdown", m_ClassName);
	}

	bool VulkanSwapchain::Recreate()
	{
		// Caller is responsible for making sure the gpu is no longer using the swapchain
		DestroySwapchain();
		if (!CreateSwapchain())
			return false;
		if (!CreateImageViews())
			return false;

		return true;
	}

	bool VulkanSwapchain::CreateSwapchain()
	{
		SwapChainSupportDetails swapChainSupport = 
			QuerySwapChainSupport(m_Context.PhysicalDevice, m_Context.Surface);

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.Formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.PresentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupport.Capabilities, m_Context.Window);

		uint32_t imageCount = swapChainSupport.Capabilities.minImageCount + 1;
		if (swapChainSupport.Capabilities.maxImageCount > 0 && 
			imageCount > swapChainSupport.Capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.Capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_Context.Surface;

		createInfo.flags = 0;
		// Will need this for VK_ERROR_OUT_OF_DATE_KHR, VK_SUBOPTIMAL_KHR
		createInfo.oldSwapchain = VK_NULL_HANDLE;
		createInfo.pNext = nullptr;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = VulkanDevice::FindQueueFamilies(m_Context.PhysicalDevice, m_Context.Surface);
		uint32_t queueFamilyIndices[] = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (indices.GraphicsFamily != indices.PresentFamily)
		{
			// will likely be better to use VK_SHARING_MODE_EXCLUSIVE in future
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}

		createInfo.preTransform = swapChainSupport.Capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		if (vkCreateSwapchainKHR(m_Context.Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create Swapchain", m_ClassName);
			return false;
		}

		vkGetSwapchainImagesKHR(m_Context.Device, m_Swapchain, &imageCount, nullptr);
		m_SwapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Context.Device, m_Swapchain, &imageCount, m_SwapchainImages.data());
		m_SwapchainFormat = surfaceFormat.format;
		m_SwapchainExtent = extent;

		return true;
	}

	void VulkanSwapchain::DestroySwapchain()
	{
		for (VkImageView imageView : m_SwapchainImageViews)
		{
			vkDestroyImageView(m_Context.Device, imageView, nullptr);
		}
		m_SwapchainImageViews.clear();
		if (m_Swapchain)
			vkDestroySwapchainKHR(m_Context.Device, m_Swapchain, nullptr);
		m_SwapchainImages.clear();
		m_Swapchain = VK_NULL_HANDLE;
	}

	bool VulkanSwapchain::CreateImageViews()
	{
		m_SwapchainImageViews.resize(m_SwapchainImages.size());

		for (uint32_t i = 0; i < m_SwapchainImages.size(); i++)
		{
			std::optional<VkImageView> ImageView =
				CreateImageView(m_Context.Device, m_SwapchainImages[i], m_SwapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
			if (!ImageView.has_value())
			{
				for (uint32_t j = 0; j < i; j++)
				{
					vkDestroyImageView(m_Context.Device, m_SwapchainImageViews[j], nullptr);
				}
				m_SwapchainImageViews.clear();

				return false;
			}

			m_SwapchainImageViews[i] = ImageView.value();
		}
		return true;
	}

	std::optional<VkImageView> VulkanSwapchain::CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create image view", m_ClassName);
			return std::nullopt;
		}

		return imageView;
	}

	SwapChainSupportDetails VulkanSwapchain::QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.Capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.Formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.PresentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR VulkanSwapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR VulkanSwapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		bool immediateMode = false;
		bool mailboxMode = false;

		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				mailboxMode = true;
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
				immediateMode = true;
		}

		if (m_VSync)
		{
			if (mailboxMode)
				return VK_PRESENT_MODE_MAILBOX_KHR;
			return VK_PRESENT_MODE_FIFO_KHR;
		}

		if (immediateMode)
			return VK_PRESENT_MODE_IMMEDIATE_KHR;
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanSwapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* const window)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D actualExtent =
			{
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}
}
