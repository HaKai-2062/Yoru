#include "Yoru/Renderer/Mesh.h"
#include "Yoru/Renderer/MeshLoader.h"
#include "Yoru/RHI/RHIDevice.h"
#include "Yoru/RHI/RHICommandList.h"
#include "Yoru/Core/Log.h"

namespace Yoru
{
	std::unique_ptr<Mesh> Mesh::Create(RHIDevice& device, std::span<const Vertex> vertices,
		std::span<const uint32_t> indices, std::vector<Submesh> submeshes)
	{
		if (vertices.empty() || indices.empty())
		{
			Log::Write(LogLevel::ERROR, "Mesh: Needs at least one vertex and one index");
			return nullptr;
		}

		std::unique_ptr<Mesh> mesh(new Mesh());
		mesh->m_VertexCount = static_cast<uint32_t>(vertices.size());
		mesh->m_IndexCount = static_cast<uint32_t>(indices.size());
		mesh->m_Submeshes = submeshes.empty()
			? std::vector<Submesh>{ { 0, mesh->m_IndexCount } }
			: std::move(submeshes);

		BufferDesc vertexDesc{};
		vertexDesc.Size = vertices.size_bytes();
		vertexDesc.Usage = BufferUsage::Vertex;
		vertexDesc.InitialData = vertices.data();
		mesh->m_VertexBuffer = device.CreateBuffer(vertexDesc);

		BufferDesc indexDesc{};
		indexDesc.Size = indices.size_bytes();
		indexDesc.Usage = BufferUsage::Index;
		indexDesc.InitialData = indices.data();
		mesh->m_IndexBuffer = device.CreateBuffer(indexDesc);

		if (!mesh->m_VertexBuffer || !mesh->m_IndexBuffer)
			return nullptr;

		return mesh;
	}

	std::unique_ptr<Mesh> Mesh::Load(RHIDevice& device, const std::filesystem::path& path)
	{
		MeshData data;
		if (!LoadGLTF(path, data))
			return nullptr;

		return Create(device, data.Vertices, data.Indices, std::move(data.Submeshes));
	}

	Mesh::~Mesh() = default;

	void Mesh::Draw(RHICommandList& cmd) const
	{
		cmd.BindIndexBuffer(*m_IndexBuffer, IndexType::UInt32);
		for (const Submesh& submesh : m_Submeshes)
			cmd.DrawIndexed(submesh.IndexCount, 1, submesh.FirstIndex);
	}

	uint64_t Mesh::GetVertexBufferAddress() const
	{
		return m_VertexBuffer->GetDeviceAddress();
	}
}
