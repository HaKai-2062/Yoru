#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace Yoru
{
	class RHIDevice;
	class RHIBuffer;
	class RHICommandList;

	// Matches the std430 Vertex struct shaders read through the buffer address
	struct Vertex
	{
		glm::vec3 Position = glm::vec3(0.0f);
		float UVX = 0.0f;
		glm::vec3 Normal = glm::vec3(0.0f, 1.0f, 0.0f);
		float UVY = 0.0f;
		glm::vec4 Color = glm::vec4(1.0f);
	};

	struct Submesh
	{
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
	};

	class Mesh
	{
	public:
		// Empty submeshes means a single submesh over all indices
		static std::unique_ptr<Mesh> Create(RHIDevice& device, std::span<const Vertex> vertices,
			std::span<const uint32_t> indices, std::vector<Submesh> submeshes = {});
		// glTF/glb through fastgltf, node transforms are baked into the vertices
		static std::unique_ptr<Mesh> Load(RHIDevice& device, const std::filesystem::path& path);

		~Mesh();

		// Binds the index buffer and draws every submesh. Bind a pipeline and push
		// GetVertexBufferAddress() before calling this.
		void Draw(RHICommandList& cmd) const;

		uint64_t GetVertexBufferAddress() const;
		uint32_t GetVertexCount() const { return m_VertexCount; }
		uint32_t GetIndexCount() const { return m_IndexCount; }
		const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }

	private:
		Mesh() = default;

	private:
		std::unique_ptr<RHIBuffer> m_VertexBuffer;
		std::unique_ptr<RHIBuffer> m_IndexBuffer;
		std::vector<Submesh> m_Submeshes;
		uint32_t m_VertexCount = 0;
		uint32_t m_IndexCount = 0;
	};
}
