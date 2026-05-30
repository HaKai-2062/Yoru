#pragma once

#include <memory>
#include "glm/glm.hpp"

namespace Yoru
{
	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, Vulkan = 1, DirectX12 = 2
		};

	public:
		virtual ~RendererAPI() = default;
		virtual void Init() = 0;
		virtual void Update() = 0;
		virtual void Shutdown() = 0;
		
		//virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		//virtual void SetClearColor(const glm::vec4& color) = 0;
		//virtual void ClearBuffers() = 0;
		//virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
		//virtual void DrawLines(const std::shared_ptr<VertexArray>& vertexArray, uint32_t vertexCount) = 0;
		//virtual void SetLineWidth(float width) = 0;

		static std::unique_ptr<RendererAPI> Create(API api);

	public:
		static API s_API;
	};
}