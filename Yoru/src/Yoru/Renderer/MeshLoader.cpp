#include "Yoru/Renderer/MeshLoader.h"
#include "Yoru/Core/Log.h"

#include <glm/gtc/type_ptr.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <cstring>

namespace Yoru
{
	static void AppendPrimitive(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive,
		const glm::mat4& transform, MeshData& outData)
	{
		auto position = primitive.findAttribute("POSITION");
		if (position == primitive.attributes.end() || !primitive.indicesAccessor.has_value())
			return;

		const uint32_t baseVertex = static_cast<uint32_t>(outData.Vertices.size());
		const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

		const fastgltf::Accessor& positionAccessor = asset.accessors[position->accessorIndex];
		outData.Vertices.resize(baseVertex + positionAccessor.count);
		fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor,
			[&](glm::vec3 v, size_t index)
			{
				outData.Vertices[baseVertex + index].Position = glm::vec3(transform * glm::vec4(v, 1.0f));
			});

		auto normal = primitive.findAttribute("NORMAL");
		if (normal != primitive.attributes.end())
		{
			fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normal->accessorIndex],
				[&](glm::vec3 n, size_t index)
				{
					outData.Vertices[baseVertex + index].Normal = glm::normalize(normalMatrix * n);
				});
		}

		auto uv = primitive.findAttribute("TEXCOORD_0");
		if (uv != primitive.attributes.end())
		{
			fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uv->accessorIndex],
				[&](glm::vec2 v, size_t index)
				{
					outData.Vertices[baseVertex + index].UVX = v.x;
					outData.Vertices[baseVertex + index].UVY = v.y;
				});
		}

		auto color = primitive.findAttribute("COLOR_0");
		if (color != primitive.attributes.end())
		{
			const fastgltf::Accessor& colorAccessor = asset.accessors[color->accessorIndex];
			if (colorAccessor.type == fastgltf::AccessorType::Vec3)
			{
				fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, colorAccessor,
					[&](glm::vec3 c, size_t index) { outData.Vertices[baseVertex + index].Color = glm::vec4(c, 1.0f); });
			}
			else
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, colorAccessor,
					[&](glm::vec4 c, size_t index) { outData.Vertices[baseVertex + index].Color = c; });
			}
		}

		const fastgltf::Accessor& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];
		Submesh submesh{};
		submesh.FirstIndex = static_cast<uint32_t>(outData.Indices.size());
		submesh.IndexCount = static_cast<uint32_t>(indexAccessor.count);

		outData.Indices.reserve(outData.Indices.size() + indexAccessor.count);
		fastgltf::iterateAccessor<uint32_t>(asset, indexAccessor,
			[&](uint32_t index) { outData.Indices.push_back(baseVertex + index); });

		outData.Submeshes.push_back(submesh);
	}

	bool LoadGLTF(const std::filesystem::path& path, MeshData& outData)
	{
		auto data = fastgltf::GltfDataBuffer::FromPath(path);
		if (data.error() != fastgltf::Error::None)
		{
			Log::Write(LogLevel::ERROR, "MeshLoader: Failed to read {}", path.string());
			return false;
		}

		constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
			fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices;

		fastgltf::Parser parser{};
		auto asset = parser.loadGltf(data.get(), path.parent_path(), options);
		if (asset.error() != fastgltf::Error::None)
		{
			Log::Write(LogLevel::ERROR, "MeshLoader: Failed to parse {}: {}", path.string(),
				fastgltf::getErrorMessage(asset.error()));
			return false;
		}

		outData = {};
		const fastgltf::Asset& gltf = asset.get();

		if (gltf.scenes.empty())
		{
			// No scene graph, take the meshes as they are
			for (const fastgltf::Mesh& mesh : gltf.meshes)
				for (const fastgltf::Primitive& primitive : mesh.primitives)
					AppendPrimitive(gltf, primitive, glm::mat4(1.0f), outData);
		}
		else
		{
			const size_t sceneIndex = gltf.defaultScene.value_or(0);
			fastgltf::iterateSceneNodes(gltf, sceneIndex, fastgltf::math::fmat4x4(),
				[&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix)
				{
					if (!node.meshIndex.has_value())
						return;

					// Both are column major
					glm::mat4 transform;
					std::memcpy(glm::value_ptr(transform), matrix.data(), sizeof(transform));

					for (const fastgltf::Primitive& primitive : gltf.meshes[*node.meshIndex].primitives)
						AppendPrimitive(gltf, primitive, transform, outData);
				});
		}

		if (outData.Vertices.empty() || outData.Indices.empty())
		{
			Log::Write(LogLevel::ERROR, "MeshLoader: {} has no triangle data", path.string());
			return false;
		}

		Log::Write(LogLevel::INFO, "MeshLoader: Loaded {} ({} vertices, {} submeshes)",
			path.filename().string(), outData.Vertices.size(), outData.Submeshes.size());
		return true;
	}
}
