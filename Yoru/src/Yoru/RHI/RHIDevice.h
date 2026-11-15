#pragma once

#include "Yoru/RHI/RHICommon.h"
#include "Yoru/RHI/RHIShader.h"
#include "Yoru/RHI/RHIBuffer.h"

#include <memory>

namespace Yoru
{
	class RHIInstance;
	class RHISwapchain;

	class RHIDevice
	{
	public:
		RHIDevice() = default;
		virtual ~RHIDevice() = default;

		virtual bool Init(const DeviceDesc& desc) = 0;
		virtual void Shutdown() = 0;

		virtual RHISwapchain* CreateSwapchain(const SwapchainDesc& desc) = 0;
		// Both return nullptr on failure
		virtual std::unique_ptr<RHIShader> CreateShader(const ShaderDesc& desc) = 0;
		virtual std::unique_ptr<RHIBuffer> CreateBuffer(const BufferDesc& desc) = 0;
		//virtual RHITexture* CreateTexture(const TextureDesc&) = 0;
		//virtual RHISampler* CreateSampler(const SamplerDesc&) = 0;
	};
}
