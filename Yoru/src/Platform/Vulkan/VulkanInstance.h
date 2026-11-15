#pragma once

#include "Yoru/RHI/RHIInstance.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanTypes.h"

struct GLFWwindow;
namespace Yoru
{
	// Instance/DebugMessenger
	class VulkanInstance : public RHIInstance
	{
	public:
		VulkanInstance() = default;
		~VulkanInstance() = default;

		virtual bool Init(const InstanceDesc& desc) override;
		virtual void Shutdown() override;
		virtual VulkanDevice* CreateDevice(const DeviceDesc& desc);

		const uint32_t GetVersion() { return m_APIVersion; }
		const VkInstance GetInstance() const { return m_Instance; }
		const VkSurfaceKHR GetSurface() const { return m_Surface; }
		GLFWwindow* GetWindow() const { return m_Window; }
		//const std::vector<const char*>& GetValidationLayers() const { return m_ValidationLayers; }
		//const bool IsValidationLayerEnabled() const { return m_EnableValidationLayers; }

	private:
		bool CreateInstance();
		bool SetupDebugMessenger();
		bool CreateSurface();
		bool CheckValidationLayerSupport();
		std::vector<const char*> GetRequiredExtensions();
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	private:
		static inline const char* m_ClassName = "VulkanInstance";
		bool m_EnableValidationLayers = false;
		bool m_HasInitialized = false;

		GLFWwindow* m_Window = nullptr;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		const std::vector<const char*> m_ValidationLayers =
		{
			"VK_LAYER_KHRONOS_validation"
		};
		uint32_t m_APIVersion = VK_API_VERSION_1_3;
	};
}