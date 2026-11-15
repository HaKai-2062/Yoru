#pragma once

#include "Yoru/RHI/RHICommon.h"

namespace Yoru
{
	class RHISwapchain
	{
	public:
		RHISwapchain() = default;
		virtual ~RHISwapchain() = default;

		virtual bool Init(const SwapchainDesc& desc) = 0;
		virtual void Shutdown() = 0;
	};
}