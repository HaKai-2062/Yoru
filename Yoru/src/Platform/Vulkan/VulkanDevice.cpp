#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanInstance.h"
#include "Platform/Vulkan/VulkanInitializers.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanBuffer.h"
#include "Yoru/Core/Application.h"
#include "Yoru/Core/Log.h"

#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include "pch.h"

namespace Yoru
{
	bool VulkanDevice::Init(const DeviceDesc& desc)
	{
		m_Context.Instance = m_VulkanInstance->GetInstance();
		m_Context.Surface = m_VulkanInstance->GetSurface();
		m_Context.Window = m_VulkanInstance->GetWindow();

		if (!PickPhysicalDevice())
			return false;
		if (!CreateLogicalDevice())
			return false;
		if (!CreateAllocator())
			return false;
		if (!CreateImmediateCommands())
			return false;

		m_Context.Device = m_Device;
		m_Context.PhysicalDevice = m_PhysicalDevice;
		m_Context.GraphicsQueue = m_GraphicsQueue;
		m_Context.PresentQueue = m_PresentQueue;
		m_Context.GraphicsQueueFamily = m_QueueIndices.GraphicsFamily.value();
		m_Context.Allocator = m_Allocator;

		Log::Write(LogLevel::INFO, "{}: Init", m_ClassName);
		//PrintPhysicalDeviceInfo(m_PhysicalDevice);

		m_HasInitialized = true;
		return true;
	}

	void VulkanDevice::Shutdown()
	{
		if (!m_HasInitialized)
			return;
		
		if (m_Device)
		{
			vkDeviceWaitIdle(m_Device);
			m_MainDeletionQueue.Flush();
			if (m_ImmediateFence)
				vkDestroyFence(m_Device, m_ImmediateFence, nullptr);
			if (m_ImmediatePool)
				vkDestroyCommandPool(m_Device, m_ImmediatePool, nullptr);
			if (m_Allocator)
				vmaDestroyAllocator(m_Allocator);
			vkDestroyDevice(m_Device, nullptr);
		}

		m_Context = {};
		m_HasInitialized = false;
		m_QueueIndices = {};
		m_PhysicalDevice = VK_NULL_HANDLE;
		m_Device = VK_NULL_HANDLE;
		m_GraphicsQueue = VK_NULL_HANDLE;
		m_PresentQueue = VK_NULL_HANDLE;
		m_Allocator = VK_NULL_HANDLE;
		m_ImmediatePool = VK_NULL_HANDLE;
		m_ImmediateCommandBuffer = VK_NULL_HANDLE;
		m_ImmediateFence = VK_NULL_HANDLE;

		Log::Write(LogLevel::INFO, "{}: Shutdown", m_ClassName);
	}

	VulkanSwapchain* VulkanDevice::CreateSwapchain(const SwapchainDesc& desc)
	{
		VulkanSwapchain* vkSwapchain = new VulkanSwapchain;
		vkSwapchain->SetContext(GetContext());

		if (vkSwapchain->Init(desc))
		{
			return vkSwapchain;
		}

		delete vkSwapchain;
		return nullptr;
	}

	std::unique_ptr<RHIShader> VulkanDevice::CreateShader(const ShaderDesc& desc)
	{
		auto shader = std::make_unique<VulkanShader>(this, desc);
		if (!shader->GetModule())
			return nullptr;
		return shader;
	}

	std::unique_ptr<RHIBuffer> VulkanDevice::CreateBuffer(const BufferDesc& desc)
	{
		auto buffer = std::make_unique<VulkanBuffer>();
		if (!buffer->Init(this, desc))
			return nullptr;
		return buffer;
	}

	void VulkanDevice::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
	{
		VK_CHECK(vkResetFences(m_Device, 1, &m_ImmediateFence));
		VK_CHECK(vkResetCommandBuffer(m_ImmediateCommandBuffer, 0));

		VkCommandBufferBeginInfo beginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		VK_CHECK(vkBeginCommandBuffer(m_ImmediateCommandBuffer, &beginInfo));
		function(m_ImmediateCommandBuffer);
		VK_CHECK(vkEndCommandBuffer(m_ImmediateCommandBuffer));

		VkCommandBufferSubmitInfo cmdInfo = VkInit::commandBufferSubmitInfo(m_ImmediateCommandBuffer);
		VkSubmitInfo2 submit = VkInit::submitInfo(&cmdInfo, nullptr, nullptr);
		VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, m_ImmediateFence));
		VK_CHECK(vkWaitForFences(m_Device, 1, &m_ImmediateFence, VK_TRUE, UINT64_MAX));
	}

	bool VulkanDevice::CreateImmediateCommands()
	{
		VkCommandPoolCreateInfo poolInfo = VkInit::commandPoolCreateInfo(
			m_QueueIndices.GraphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		VkFenceCreateInfo fenceInfo = VkInit::fenceCreateInfo();

		if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_ImmediatePool) != VK_SUCCESS)
			return false;

		VkCommandBufferAllocateInfo allocInfo = VkInit::commandBufferAllocateInfo(m_ImmediatePool);
		if (vkAllocateCommandBuffers(m_Device, &allocInfo, &m_ImmediateCommandBuffer) != VK_SUCCESS ||
			vkCreateFence(m_Device, &fenceInfo, nullptr, &m_ImmediateFence) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create immediate submit objects", m_ClassName);
			return false;
		}

		return true;
	}

	bool VulkanDevice::PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_Context.Instance, &deviceCount, nullptr);
		if (deviceCount == 0)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to find GPUs with Vulkan support", m_ClassName);
			return false;
		}

		// Go through all gpus and rearrange them 
		// according to discrete gpus first then other gpus
		std::deque<VkPhysicalDevice> sortedDevices;
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_Context.Instance, &deviceCount, devices.data());
		for (const auto& device : devices)
		{
			VkPhysicalDeviceProperties props{};
			vkGetPhysicalDeviceProperties(device, &props);
			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				sortedDevices.push_front(device);
			else
				sortedDevices.push_back(device);
		}

		// Finds first gpu in our sorted list that fullfils our reqs
		for (const auto& device : sortedDevices)
		{
			if (IsDeviceSuitable(device, m_Context.Surface))
			{
				m_PhysicalDevice = device;
				//m_MSAASamples = GetMaxUsableSampleCount();
				break;
			}
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to find a suitable GPU", m_ClassName);
			return false;
		}
		return true;
	}

	bool VulkanDevice::CreateLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice, m_Context.Surface);
		m_QueueIndices = indices;

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		// Features
		VkPhysicalDeviceVulkan13Features features13{};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		features13.dynamicRendering = VK_TRUE; // removes renderpass
		features13.synchronization2 = VK_TRUE; // better sync
		features13.pNext = nullptr;

		VkPhysicalDeviceVulkan12Features features12{};
		features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features12.bufferDeviceAddress = VK_TRUE; // Getting pointer to gpu memory
		features12.timelineSemaphore = VK_TRUE;	// Multiple frame in flight streamlined
		//features12.runtimeDescriptorArray = VK_TRUE;
		//features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
		//features12.descriptorBindingPartiallyBound = VK_TRUE;
		features12.pNext = &features13;

		VkPhysicalDeviceFeatures2 features2{};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.features.samplerAnisotropy = VK_TRUE;
		features2.pNext = &features12;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = nullptr;	// Modern vulkan uses pNext
		createInfo.pNext = &features2;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
		createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();
		createInfo.enabledLayerCount = 0;

		if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create Logical Device", m_ClassName);
			return false;
		}
		volkLoadDevice(m_Device);

		vkGetDeviceQueue(m_Device, indices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
		if (!m_GraphicsQueue)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to get graphics queue", m_ClassName);
			return false;
		}
		vkGetDeviceQueue(m_Device, indices.PresentFamily.value(), 0, &m_PresentQueue);
		if (!m_PresentQueue)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to get present queue", m_ClassName);
			return false;
		}

		return true;
	}

	bool VulkanDevice::CreateAllocator()
	{
		VmaVulkanFunctions vmaFuncs = {};
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = m_PhysicalDevice;
		allocatorInfo.device = m_Device;
		allocatorInfo.instance = m_Context.Instance;
		// To use GPU pointers
		allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		allocatorInfo.pVulkanFunctions = &vmaFuncs;

		// import vulkan functions from volk
		vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vmaFuncs);

		if (vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "{}: Failed to create Vulkan Memory Allocator", m_ClassName);
			return false;
		}

		return true;
	}

	QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.GraphicsFamily = i;

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport)
				indices.PresentFamily = i;

			if (indices.IsComplete())
				break;

			i++;
		}

		return indices;
	}

	bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices = FindQueueFamilies(device, surface);

		bool extensionsSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = 
				VulkanSwapchain::QuerySwapChainSupport(device, surface);
			swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
		}

		bool featuresSupported = CheckDeviceFeatureSupport(device);

		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(device, &props);
		bool supportsVkAPI = props.apiVersion >= m_VulkanInstance->GetVersion();

		return supportsVkAPI && indices.IsComplete() && extensionsSupported &&
			swapChainAdequate && featuresSupported;
	}

	bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	bool VulkanDevice::CheckDeviceFeatureSupport(VkPhysicalDevice device)
	{
		// These are all core features so they use VkPhysicalDeviceVulkanXXFeatures
		VkPhysicalDeviceVulkan13Features supportedFeatures13
		{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = nullptr };
		VkPhysicalDeviceVulkan12Features supportedFeatures12
		{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &supportedFeatures13 };
		VkPhysicalDeviceFeatures2 supportedFeatures2
		{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &supportedFeatures12 };
		vkGetPhysicalDeviceFeatures2(device, &supportedFeatures2);

		if (supportedFeatures13.dynamicRendering && supportedFeatures13.synchronization2 &&
			supportedFeatures12.timelineSemaphore && supportedFeatures12.bufferDeviceAddress &&
			supportedFeatures2.features.samplerAnisotropy)
			//&& supportedFeatures12.runtimeDescriptorArray &&
			//supportedFeatures12.descriptorBindingVariableDescriptorCount && supportedFeatures12.descriptorBindingPartiallyBound)
		{
			return true;
		}

		return false;
	}

	VkSampleCountFlagBits VulkanDevice::GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice)
	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}

	void VulkanDevice::PrintPhysicalDeviceInfo(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(device, &props);

		VkPhysicalDeviceMemoryProperties memProps{};
		vkGetPhysicalDeviceMemoryProperties(device, &memProps);

		Log::Write(LogLevel::TRACE, "================ GPU INFO ================");
		Log::Write(LogLevel::TRACE, "Device Name      : {}", props.deviceName);
		Log::Write(LogLevel::TRACE, "Vendor ID        : {}", props.vendorID);
		Log::Write(LogLevel::TRACE, "Device ID        : {}", props.deviceID);
		Log::Write(LogLevel::TRACE, "API Version      : {}.{}.{}",
			VK_VERSION_MAJOR(props.apiVersion),
			VK_VERSION_MINOR(props.apiVersion),
			VK_VERSION_PATCH(props.apiVersion));

		const char* typeStr = "Unknown";
		switch (props.deviceType)
		{
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			typeStr = "Integrated GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			typeStr = "Discrete GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			typeStr = "Virtual GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			typeStr = "CPU"; break;
		default:
			break;
		}

		Log::Write(LogLevel::TRACE, "Device Type      : {}", typeStr);

		VkDeviceSize totalVRAM = 0;
		for (uint32_t i = 0; i < memProps.memoryHeapCount; i++)
		{
			if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			{
				totalVRAM += memProps.memoryHeaps[i].size;
			}
		}

		// Not counting for shared vram here
		Log::Write(LogLevel::TRACE, "Device Local VRAM (MB)        : {}", totalVRAM / 1024 / 1024);

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queues(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queues.data());

		Log::Write(LogLevel::TRACE, "----- Queue Families -----");

		for (uint32_t i = 0; i < queueFamilyCount; i++)
		{
			const auto& q = queues[i];

			Log::Write(LogLevel::TRACE, "Queue Family {}", i);
			Log::Write(LogLevel::TRACE, "  Graphics : {}", (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) ? "Yes" : "No");
			Log::Write(LogLevel::TRACE, "  Compute  : {}", (q.queueFlags & VK_QUEUE_COMPUTE_BIT) ? "Yes" : "No");
			Log::Write(LogLevel::TRACE, "  Transfer : {}", (q.queueFlags & VK_QUEUE_TRANSFER_BIT) ? "Yes" : "No");
			Log::Write(LogLevel::TRACE, "  Sparse   : {}", (q.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) ? "Yes" : "No");
		}

		// Features
		VkPhysicalDeviceFeatures features{};
		vkGetPhysicalDeviceFeatures(device, &features);

		Log::Write(LogLevel::TRACE, "----- Key Features -----");
		Log::Write(LogLevel::TRACE, "Anisotropy      : {}", features.samplerAnisotropy ? "Yes" : "No");
		Log::Write(LogLevel::TRACE, "Geometry Shader : {}", features.geometryShader ? "Yes" : "No");
		Log::Write(LogLevel::TRACE, "Tessellation    : {}", features.tessellationShader ? "Yes" : "No");

		Log::Write(LogLevel::TRACE, "==========================================");
	}
}

