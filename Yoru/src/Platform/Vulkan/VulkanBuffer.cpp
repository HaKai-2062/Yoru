#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"

#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

namespace Yoru
{
	static VkBufferUsageFlags ToVulkanUsage(BufferUsage usage)
	{
		VkBufferUsageFlags flags = 0;
		if (HasFlag(usage, BufferUsage::Vertex) || HasFlag(usage, BufferUsage::Storage))
			flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (HasFlag(usage, BufferUsage::Index))
			flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (HasFlag(usage, BufferUsage::Indirect))
			flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		if (HasFlag(usage, BufferUsage::Uniform))
			flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		// Everything readable from shaders is accessed through its device address
		if (flags & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
			flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		return flags;
	}

	VulkanBuffer::~VulkanBuffer()
	{
		if (m_Buffer)
			vmaDestroyBuffer(m_Device->GetAllocator(), m_Buffer, m_Allocation);
	}

	bool VulkanBuffer::Init(VulkanDevice* device, const BufferDesc& desc)
	{
		m_Device = device;
		m_Size = desc.Size;
		m_Usage = desc.Usage;

		const bool needsStaging = desc.Memory == MemoryLocation::GPUOnly && desc.InitialData;

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = desc.Size;
		bufferInfo.usage = ToVulkanUsage(desc.Usage) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		if (desc.Memory == MemoryLocation::CPUToGPU)
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		else if (desc.Memory == MemoryLocation::GPUToCPU)
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		else
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		VmaAllocationInfo info{};
		if (vmaCreateBuffer(m_Device->GetAllocator(), &bufferInfo, &allocInfo, &m_Buffer, &m_Allocation, &info) != VK_SUCCESS)
		{
			Log::Write(LogLevel::ERROR, "{}: Failed to create buffer of size {}", m_ClassName, desc.Size);
			return false;
		}
		m_MappedData = info.pMappedData;

		if (bufferInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		{
			VkBufferDeviceAddressInfo addressInfo{};
			addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
			addressInfo.buffer = m_Buffer;
			m_DeviceAddress = vkGetBufferDeviceAddress(m_Device->GetDevice(), &addressInfo);
		}

		if (!desc.InitialData)
			return true;

		if (!needsStaging)
		{
			memcpy(m_MappedData, desc.InitialData, desc.Size);
			return true;
		}

		VkBufferCreateInfo stagingInfo{};
		stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingInfo.size = desc.Size;
		stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo stagingAllocInfo{};
		stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		VkBuffer staging = VK_NULL_HANDLE;
		VmaAllocation stagingAllocation = VK_NULL_HANDLE;
		VmaAllocationInfo stagingMapped{};
		if (vmaCreateBuffer(m_Device->GetAllocator(), &stagingInfo, &stagingAllocInfo,
			&staging, &stagingAllocation, &stagingMapped) != VK_SUCCESS)
		{
			Log::Write(LogLevel::ERROR, "{}: Failed to create staging buffer", m_ClassName);
			return false;
		}
		memcpy(stagingMapped.pMappedData, desc.InitialData, desc.Size);

		// Blocks until the copy is done, fine for load time uploads
		m_Device->ImmediateSubmit([&](VkCommandBuffer cmd)
			{
				VkBufferCopy copy{};
				copy.size = desc.Size;
				vkCmdCopyBuffer(cmd, staging, m_Buffer, 1, &copy);
			});

		vmaDestroyBuffer(m_Device->GetAllocator(), staging, stagingAllocation);
		return true;
	}
}
