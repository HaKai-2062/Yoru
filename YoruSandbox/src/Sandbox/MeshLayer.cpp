#include "MeshLayer.h"

#include <Yoru/Core/Application.h>
#include <Yoru/Core/Log.h>
#include <Yoru/RHI/RHIRenderer.h>

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <array>

MeshLayer::MeshLayer()
	: Yoru::Layer("MeshLayer")
{
}

MeshLayer::~MeshLayer() = default;

void MeshLayer::OnAttach()
{
	Yoru::RHIRenderer& renderer = Yoru::Application::Get()->GetRenderer();
	Yoru::RHIDevice& device = renderer.GetDevice();

	m_VertexShader = device.CreateShader({ Yoru::ShaderStage::Vertex, SANDBOX_SHADER_PATH "mesh.vert.spv" });
	m_FragmentShader = device.CreateShader({ Yoru::ShaderStage::Fragment, SANDBOX_SHADER_PATH "mesh.frag.spv" });
	if (!m_VertexShader || !m_FragmentShader)
		return;

	Yoru::GraphicsPipelineDesc pipelineDesc{};
	pipelineDesc.VertexShader = m_VertexShader.get();
	pipelineDesc.FragmentShader = m_FragmentShader.get();
	pipelineDesc.Cull = Yoru::CullMode::None;
	pipelineDesc.PushConstantSize = sizeof(MeshPushConstants);
	m_Pipeline = renderer.CreateGraphicsPipeline(pipelineDesc);

	// Mesh from vertices supplied here
	const std::array<Yoru::Vertex, 3> triangleVertices =
	{ {
		{ .Position = { -0.5f, -0.5f, 0.0f }, .Normal = { 0.0f, 0.0f, 1.0f }, .Color = { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ .Position = {  0.5f, -0.5f, 0.0f }, .Normal = { 0.0f, 0.0f, 1.0f }, .Color = { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ .Position = {  0.0f,  0.5f, 0.0f }, .Normal = { 0.0f, 0.0f, 1.0f }, .Color = { 0.0f, 0.0f, 1.0f, 1.0f } },
	} };
	const std::array<uint32_t, 3> triangleIndices = { 0, 1, 2 };
	AddObject("Triangle", Yoru::Mesh::Create(device, triangleVertices, triangleIndices), { -1.5f, 0.0f, 0.0f });

	// Mesh loaded from a file by the engine
	AddObject("Monkey", Yoru::Mesh::Load(device, SANDBOX_ASSET_PATH "monkey.glb"), { 1.5f, 0.0f, 0.0f });

	//AddObject("Sponza", Yoru::Mesh::Load(device, SANDBOX_ASSET_PATH "Sponza/Sponza.gltf"), { 1.5f, 0.0f, 0.0f });
}

void MeshLayer::OnDetach()
{
	// Application waits for the GPU to go idle before detaching layers
	m_Objects.clear();
	m_Pipeline.reset();
	m_VertexShader.reset();
	m_FragmentShader.reset();
}

void MeshLayer::OnUpdate(float deltaTime)
{
	m_Camera.ProcessKeyEvents(deltaTime);
	m_Camera.ProcessMouseEvents(deltaTime);
	m_Rotation += m_RotationSpeed * deltaTime;
}

void MeshLayer::OnRender(Yoru::RHICommandList& cmd)
{
	if (!m_Pipeline)
		return;

	auto [width, height] = Yoru::Application::Get()->GetRenderer().GetRenderExtent();
	glm::mat4 projection = glm::perspective(glm::radians(60.0f),
		static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
	projection[1][1] *= -1.0f;	// Vulkan clip space has +Y pointing down
	const glm::mat4 viewProjection = projection * m_Camera.GetViewMatrix();
	const glm::vec3 lightDirection = glm::normalize(m_LightDirection);

	Yoru::RenderingDesc pass{};
	pass.Clear = m_ClearColor;
	cmd.BeginRendering(pass);
	cmd.BindPipeline(*m_Pipeline);

	for (const SceneObject& object : m_Objects)
	{
		glm::mat4 model = glm::translate(glm::mat4(1.0f), object.Position);
		model = glm::rotate(model, m_Rotation, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::scale(model, glm::vec3(object.Scale));

		MeshPushConstants push{};
		push.MVP = viewProjection * model;
		push.VertexBufferAddress = object.Mesh->GetVertexBufferAddress();
		// Lighting happens in object space, so bring the light there instead of passing the model matrix
		push.LightDirection = glm::vec4(glm::normalize(glm::inverse(glm::mat3(model)) * lightDirection), 0.0f);

		cmd.PushConstants(push);
		object.Mesh->Draw(cmd);
	}

	cmd.EndRendering();
}

void MeshLayer::OnImGuiRender()
{
	// Can be used for graph later
	m_DeltaHistory[m_DeltaOffset] = Yoru::Application::Get()->GetDeltaTime();
	m_DeltaOffset = (m_DeltaOffset + 1) % m_DeltaHistory.size();
	if (m_DeltaOffset == 0)
	{
		m_AvgDelta = 0.0f;
		for (int32_t i = 0; i < m_DeltaHistory.size(); i++)
		{
			m_AvgDelta += m_DeltaHistory[i];
		}
		m_AvgDelta /= m_DeltaHistory.size();
	}

	ImGui::Begin("Scene");
	ImGui::ColorEdit3("Clear color", &m_ClearColor.R);
	ImGui::SliderFloat("Rotation speed", &m_RotationSpeed, -3.0f, 3.0f);
	ImGui::SliderFloat3("Light direction", &m_LightDirection.x, -1.0f, 1.0f);
	ImGui::Separator();
	ImGui::Text("FrameTime: %.2fms, FPS: %.2f", m_AvgDelta * 1000.0f, 1.0f / m_AvgDelta);
	ImGui::Separator();
	for (const SceneObject& object : m_Objects)
		ImGui::Text("%s: %u vertices, %u indices", object.Name.c_str(), object.Mesh->GetVertexCount(), object.Mesh->GetIndexCount());
	ImGui::Separator();
	ImGui::TextDisabled("M toggles mouse look, WASD/QE moves");
	ImGui::Separator();
	ImGui::End();
}

void MeshLayer::AddObject(const std::string& name, std::unique_ptr<Yoru::Mesh> mesh, glm::vec3 position, float scale)
{
	if (!mesh)
	{
		Yoru::Log::Write(Yoru::LogLevel::ERROR, "MeshLayer: Failed to create {}", name);
		return;
	}
	m_Objects.push_back({ name, std::move(mesh), position, scale });
}
