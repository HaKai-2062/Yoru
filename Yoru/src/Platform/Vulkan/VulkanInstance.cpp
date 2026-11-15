#include "Platform/Vulkan/VulkanInstance.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Yoru/Core/Log.h"

#define VOLK_IMPLEMENTATION
#include <Volk/volk.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#include <GLFW/glfw3.h>

namespace Yoru
{
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

	bool VulkanInstance::Init(const InstanceDesc& desc)
	{
		m_EnableValidationLayers = desc.EnableValidationLayers;
		m_Window = static_cast<GLFWwindow*>(desc.NativeWindow);

		if (!CreateInstance())
			return false;
		if (!SetupDebugMessenger())
			return false;
		if (!CreateSurface())
			return false;

		Log::Write(LogLevel::INFO, "{}: Init", m_ClassName);

		m_HasInitialized = true;
		return true;
	}

	void VulkanInstance::Shutdown()
	{
		if (m_Instance && m_Surface)
			vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		if (m_Instance && m_DebugMessenger)
			DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
		if (m_Instance)
			vkDestroyInstance(m_Instance, nullptr);
		volkFinalize();

		m_HasInitialized = false;
		m_Window = VK_NULL_HANDLE;
		m_Surface = VK_NULL_HANDLE;
		m_Instance = VK_NULL_HANDLE;
		m_DebugMessenger = VK_NULL_HANDLE;

		Log::Write(LogLevel::INFO, "{}: Shutdown", m_ClassName);
	}

	VulkanDevice* VulkanInstance::CreateDevice(const DeviceDesc& desc)
	{
		VulkanDevice* vkDevice = new VulkanDevice;
		// Device can set the stuff it needs from instance
		vkDevice->SetVulkanInstance(this);
		if (vkDevice->Init(desc))
		{
			return vkDevice;
		}

		delete vkDevice;
		return nullptr;
	}

	bool VulkanInstance::CreateInstance()
	{
		if (volkInitialize() != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to initialize Volk", m_ClassName);
			return false;
		}

		if (m_EnableValidationLayers && !CheckValidationLayerSupport())
		{
			Log::Write(LogLevel::FATAL, "{}: Requested validation layers are not availabe", m_ClassName);
			return false;
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan Application";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Vulkan GameEngine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = m_APIVersion;

		auto extensions = GetRequiredExtensions();
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

		if (m_EnableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
			createInfo.ppEnabledLayerNames = m_ValidationLayers.data();

			PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create Vulkan Instance", m_ClassName);
			return false;
		}

		volkLoadInstance(m_Instance);
		return true;
	}

	bool VulkanInstance::SetupDebugMessenger()
	{
		if (!m_EnableValidationLayers)
			return true;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to setup debug messenger", m_ClassName);
			return false;
		}

		return true;
	}

	bool VulkanInstance::CreateSurface()
	{
		if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create window surface", m_ClassName);
			return false;
		}

		return true;
	}

	bool VulkanInstance::CheckValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : m_ValidationLayers)
		{
			bool layerFound = false;
			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}

		return true;
	}

	std::vector<const char*> VulkanInstance::GetRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (m_EnableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	void VulkanInstance::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
	}

	VkResult VulkanInstance::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void VulkanInstance::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		const char* fnName = "vkDestroyDebugUtilsMessengerEXT";
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, fnName);
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
		}
		else if (m_EnableValidationLayers)
		{
			Log::Write(LogLevel::ERROR, "{}: Failed to call {}", m_ClassName, fnName);
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		Log::Write(LogLevel::WARN, "Validation Layer: {}", pCallbackData->pMessage);
		return VK_FALSE;
	}
}
