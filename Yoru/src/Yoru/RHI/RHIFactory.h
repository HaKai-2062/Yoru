#pragma once

#include "Yoru/RHI/RHIRenderer.h"

#include <memory>

namespace Yoru
{
	class RHIFactory
	{
	public:
		enum class API
		{
			None = 0, Vulkan = 1, DirectX12 = 2
		};

	public:
		virtual ~RHIFactory() = default;
		static std::unique_ptr<RHIRenderer> Create(API api, RendererDesc desc);

	public:
		// TDL: Remove this entirely and store it inside Application.h
		static API s_API;
	};
}