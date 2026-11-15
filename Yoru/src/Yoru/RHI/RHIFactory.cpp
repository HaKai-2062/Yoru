#include "Yoru/RHI/RHIFactory.h"
#include "Yoru/Core/Log.h"
#include "Platform/Vulkan/VulkanRenderer.h"

namespace Yoru
{
	RHIFactory::API RHIFactory::s_API = RHIFactory::API::Vulkan;

	std::unique_ptr<RHIRenderer> RHIFactory::Create(API api, RendererDesc desc)
	{
		s_API = api;
		switch (s_API)
		{
		case API::Vulkan:
		{
			std::unique_ptr<RHIRenderer> rhiRenderer = std::make_unique<VulkanRenderer>();
			return rhiRenderer;
		}
		case API::DirectX12:
		{
			Log::Write(LogLevel::ERROR, "RHIFactory: DX12 not implemented");
			return nullptr;
		}

		default:
		{
			Log::Write(LogLevel::ERROR, "RHIFactory: Unknown RHI backend");
			return nullptr;
		}
		}
	}
}