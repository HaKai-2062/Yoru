#pragma once

#include "Yoru/Renderer/Mesh.h"

#include <filesystem>
#include <vector>

namespace Yoru
{
	struct MeshData
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
		std::vector<Submesh> Submeshes;	// one per glTF primitive
	};

	// Flattens every mesh in the default scene into one vertex/index list
	bool LoadGLTF(const std::filesystem::path& path, MeshData& outData);
}
