#pragma once

#include "Yoru/RHI/RHIBuffer.h"
#include "Platform/Vulkan/VulkanTypes.h"

namespace Yoru
{
	class VulkanDevice;

	class VulkanBuffer : public RHIBuffer
	{
	public:
		VulkanBuffer() = default;
		~VulkanBuffer();
		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;

		bool Init(VulkanDevice* device, const BufferDesc& desc);

		virtual uint64_t GetSize() const override { return m_Size; }
		virtual BufferUsage GetUsage() const override { return m_Usage; }
		virtual uint64_t GetDeviceAddress() const override { return m_DeviceAddress; }
		virtual void* GetMappedData() const override { return m_MappedData; }

		VkBuffer GetBuffer() const { return m_Buffer; }

	private:
		static inline const char* m_ClassName = "VulkanBuffer";
		VulkanDevice* m_Device = nullptr;

		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = VK_NULL_HANDLE;
		void* m_MappedData = nullptr;
		uint64_t m_DeviceAddress = 0;
		uint64_t m_Size = 0;
		BufferUsage m_Usage = BufferUsage::None;
	};
}
