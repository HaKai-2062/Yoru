#include "Yoru/Core/Log.h"
#include "Yoru/Renderer/RendererAPI.h"
#include "Platform/Vulkan/VKRenderer.h"

namespace Yoru
{
	RendererAPI::API RendererAPI::s_API = RendererAPI::API::Vulkan;

	std::unique_ptr<RendererAPI> RendererAPI::Create(API api)
	{
		s_API = api;
		switch (s_API)
		{
		case API::Vulkan:
		{
			std::unique_ptr<VKRenderer> Context = std::make_unique<VKRenderer>();
			Context->Init();
			return std::move(Context);
		}
		//case API::DirectX12:
		//{
			//return std::make_unique<DX12RenderAPI>();
		//	Log::Write(LogLevel::ERROR, "DX12 Renderer not yet implemented");
		//	return nullptr;
		//}
		default:
		{
			Log::Write(LogLevel::ERROR, "Set up the RenderAPI in RendererAPI.cpp!");
			return nullptr;
		}
		}

		Log::Write(LogLevel::ERROR, "Unknown RendererAPI");
		return nullptr;
	}
}