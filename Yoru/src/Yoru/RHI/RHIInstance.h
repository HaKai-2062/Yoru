#pragma once

#include "Yoru/RHI/RHICommon.h"

namespace Yoru
{
	class RHIDevice;

	// Instance->Device->Swapchain

	class RHIInstance
	{
	public:
		RHIInstance() = default;
		virtual ~RHIInstance() = default;

		virtual bool Init(const InstanceDesc& desc) = 0;
		virtual void Shutdown() = 0;

		virtual RHIDevice* CreateDevice(const DeviceDesc& desc) = 0;
	};
}
