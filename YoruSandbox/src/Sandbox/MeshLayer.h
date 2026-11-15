#pragma once

#include <Yoru/Core/Layer.h>
#include <Yoru/RHI/RHICommon.h>
#include <Yoru/Renderer/Mesh.h>
#include <Yoru/Scene/Camera.h>

#include <glm/glm.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Yoru
{
	class RHIShader;
	class RHIPipeline;
}

// Must match the push_constant block in shaders/mesh.vert
struct MeshPushConstants
{
	glm::mat4 MVP;
	uint64_t VertexBufferAddress;
	uint64_t Padding;
	glm::vec4 LightDirection;	// object space
};

class MeshLayer : public Yoru::Layer
{
public:
	MeshLayer();
	virtual ~MeshLayer() override;

	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate(float deltaTime) override;
	virtual void OnRender(Yoru::RHICommandList& cmd) override;
	virtual void OnImGuiRender() override;

private:
	struct SceneObject
	{
		std::string Name;
		std::unique_ptr<Yoru::Mesh> Mesh;
		glm::vec3 Position = glm::vec3(0.0f);
		float Scale = 1.0f;
	};

	void AddObject(const std::string& name, std::unique_ptr<Yoru::Mesh> mesh, glm::vec3 position, float scale = 1.0f);

private:
	std::unique_ptr<Yoru::RHIShader> m_VertexShader;
	std::unique_ptr<Yoru::RHIShader> m_FragmentShader;
	std::unique_ptr<Yoru::RHIPipeline> m_Pipeline;
	std::vector<SceneObject> m_Objects;

	Yoru::Camera m_Camera{ glm::vec3(0.0f, 0.5f, 5.0f) };
	Yoru::ClearColor m_ClearColor{ 0.05f, 0.05f, 0.08f, 1.0f };
	glm::vec3 m_LightDirection = glm::vec3(-0.4f, -1.0f, -0.6f);
	float m_RotationSpeed = 0.6f;
	float m_Rotation = 0.0f;

	std::array<float, 120> m_DeltaHistory;
	int32_t m_DeltaOffset = 0;
	float m_AvgDelta = 0.0f;
};
