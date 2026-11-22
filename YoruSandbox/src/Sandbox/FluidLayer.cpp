#include "FluidLayer.h"
#include "MarchingCubesTables.h"

#include <Yoru/Core/Application.h>
#include <Yoru/Core/Log.h>
#include <Yoru/RHI/RHIRenderer.h>

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

using Usage = Yoru::ResourceUsage;

// Thread group size, must match local_size_x/y/z in field.comp and marching_cubes.comp
static constexpr uint32_t GROUP_SIZE = 4;

static uint32_t GroupCount(uint32_t threads) { return (threads + GROUP_SIZE - 1) / GROUP_SIZE; }

FluidLayer::FluidLayer()
	: Yoru::Layer("FluidLayer")
{
}

FluidLayer::~FluidLayer() = default;

void FluidLayer::OnAttach()
{
	if (!CreatePipelines())
		Yoru::Log::Write(Yoru::LogLevel::ERROR, "FluidLayer: Failed to create pipelines");
	if (!CreateBuffers())
		Yoru::Log::Write(Yoru::LogLevel::ERROR, "FluidLayer: Failed to create buffers");
}

void FluidLayer::OnDetach()
{
	// Application waits for the GPU to go idle before detaching layers
	m_GridBuffer.reset();
	m_TablesBuffer.reset();
	m_VertexBuffer.reset();
	m_ArgsBuffer.reset();
	m_CountReadback.clear();
	m_FieldPipeline.reset();
	m_MarchingCubesPipeline.reset();
	m_CounterPipeline.reset();
	m_SurfacePipeline.reset();
	m_FieldShader.reset();
	m_MarchingCubesShader.reset();
	m_CounterShader.reset();
	m_SurfaceVertexShader.reset();
	m_SurfaceFragmentShader.reset();
}

bool FluidLayer::CreatePipelines()
{
	Yoru::RHIRenderer& renderer = Yoru::Application::Get()->GetRenderer();
	Yoru::RHIDevice& device = renderer.GetDevice();

	m_FieldShader = device.CreateShader({ Yoru::ShaderStage::Compute, SANDBOX_SHADER_PATH "field.comp.spv" });
	m_MarchingCubesShader = device.CreateShader({ Yoru::ShaderStage::Compute, SANDBOX_SHADER_PATH "marching_cubes.comp.spv" });
	m_CounterShader = device.CreateShader({ Yoru::ShaderStage::Compute, SANDBOX_SHADER_PATH "counter.comp.spv" });
	m_SurfaceVertexShader = device.CreateShader({ Yoru::ShaderStage::Vertex, SANDBOX_SHADER_PATH "surface.vert.spv" });
	m_SurfaceFragmentShader = device.CreateShader({ Yoru::ShaderStage::Fragment, SANDBOX_SHADER_PATH "surface.frag.spv" });
	if (!m_FieldShader || !m_MarchingCubesShader || !m_CounterShader || !m_SurfaceVertexShader || !m_SurfaceFragmentShader)
		return false;
	 
	m_FieldPipeline = renderer.CreateComputePipeline({ m_FieldShader.get(), sizeof(FieldPushConstants) });
	m_MarchingCubesPipeline = renderer.CreateComputePipeline({ m_MarchingCubesShader.get(), sizeof(MarchingCubesPushConstants) });
	m_CounterPipeline = renderer.CreateComputePipeline({ m_CounterShader.get(), sizeof(CounterPushConstants) });

	Yoru::GraphicsPipelineDesc surfaceDesc{};
	surfaceDesc.VertexShader = m_SurfaceVertexShader.get();
	surfaceDesc.FragmentShader = m_SurfaceFragmentShader.get();
	surfaceDesc.Cull = Yoru::CullMode::Back;
	surfaceDesc.PushConstantSize = sizeof(SurfacePushConstants);
	m_SurfacePipeline = renderer.CreateGraphicsPipeline(surfaceDesc);

	return m_FieldPipeline && m_MarchingCubesPipeline && m_CounterPipeline && m_SurfacePipeline;
}

bool FluidLayer::CreateBuffers()
{
	Yoru::RHIDevice& device = Yoru::Application::Get()->GetRenderer().GetDevice();

	// m_GridBuffer is GPU only storage buffer with no initial data
	// written by field.comp and read by marching_cubes.comp
	Yoru::BufferDesc gridDesc{};
	gridDesc.Size = sizeof(float) * m_GridSize * m_GridSize * m_GridSize;
	gridDesc.Usage = Yoru::BufferUsage::Storage;
	gridDesc.InitialData = nullptr;
	m_GridBuffer = device.CreateBuffer(gridDesc);

	// m_ArgsBuffer by compute (atomicAdd on VertexCount) and read by DrawIndirect, so it needs both usages
	// InstanceCount = 1 is default so it isnt reset
	// Per frame reset via FillBuffer
	Yoru::DrawIndirectArgs initialArgs{};
	Yoru::BufferDesc argsDesc{};
	argsDesc.Size = sizeof(Yoru::DrawIndirectArgs);
	argsDesc.Usage = Yoru::BufferUsage::Storage | Yoru::BufferUsage::Indirect;
	argsDesc.InitialData = &initialArgs;
	m_ArgsBuffer = device.CreateBuffer(argsDesc);

	// m_VertexBuffer
	Yoru::BufferDesc vertDesc{};
	vertDesc.Size = sizeof(SurfaceVertex) * m_MaxVertices;
	vertDesc.Usage = Yoru::BufferUsage::Vertex;
	vertDesc.InitialData = nullptr;
	m_VertexBuffer = device.CreateBuffer(vertDesc);
	
	// m_TablesBuffer
	// Need to add proper offsets in shader
	std::vector<int32_t> packedArray;
	packedArray.reserve(MarchingCubesTable::g_EdgeTable.size() + MarchingCubesTable::g_TriTable.size());
	packedArray.insert(packedArray.end(), MarchingCubesTable::g_EdgeTable.begin(), MarchingCubesTable::g_EdgeTable.end());
	packedArray.insert(packedArray.end(), MarchingCubesTable::g_TriTable.begin(), MarchingCubesTable::g_TriTable.end());
	Yoru::BufferDesc tableDesc{};
	tableDesc.Size = packedArray.size() * sizeof(packedArray[0]);
	tableDesc.Usage = Yoru::BufferUsage::Storage;
	tableDesc.InitialData = packedArray.data();
	m_TablesBuffer = device.CreateBuffer(tableDesc);

	// m_CountReadback, host visible and mapped. Storage so counter.comp can write it through its address
	const uint32_t zero = 0;
	Yoru::BufferDesc readbackDesc{};
	readbackDesc.Size = sizeof(uint32_t);
	readbackDesc.Usage = Yoru::BufferUsage::Storage;
	readbackDesc.Memory = Yoru::MemoryLocation::GPUToCPU;
	readbackDesc.InitialData = &zero;
	m_CountReadback.clear();
	bool readbackOk = true;
	for (uint32_t i = 0; i < Yoru::Application::Get()->GetRenderer().GetFramesInFlight(); i++)
	{
		m_CountReadback.push_back(device.CreateBuffer(readbackDesc));
		readbackOk = readbackOk && m_CountReadback.back();
	}

	return m_GridBuffer && m_ArgsBuffer && m_VertexBuffer && m_TablesBuffer && readbackOk;
}

void FluidLayer::OnUpdate(float deltaTime)
{
	// The grid buffer is sized by GridSize, so resizing means recreating it. This runs before BeginFrame:
	// OnImGuiRender comes after this frame's passes were recorded, freeing buffers there would leave the
	// command buffer pointing at destroyed memory. WaitIdle covers the frames still in flight.
	if (m_PendingGridSize != m_GridSize)
	{
		Yoru::Application::Get()->GetRenderer().WaitIdle();
		m_GridSize = m_PendingGridSize;
		if (!CreateBuffers())
			Yoru::Log::Write(Yoru::LogLevel::ERROR, "FluidLayer: Failed to recreate buffers");
	}

	m_Camera.ProcessKeyEvents(deltaTime);
	m_Camera.ProcessMouseEvents(deltaTime);

	if (!m_Paused)
		m_Time += deltaTime;
}

void FluidLayer::OnRender(Yoru::RHICommandList& cmd)
{
	// BeginFrame already waited on this slot's fence, so the count the frame that last used it wrote is
	// finished. Read it before this frame records a new write into the same slot.
	uint32_t slot = Yoru::Application::Get()->GetRenderer().GetFrameIndex();
	if (slot < m_CountReadback.size() && m_CountReadback[slot])
		m_LastVertexCount = *static_cast<const uint32_t*>(m_CountReadback[slot]->GetMappedData());

	// Compute passes must stay outside BeginRendering/EndRendering, only the surface draw goes inside.
	RecordResetCounter(cmd);
	RecordDensityField(cmd);
	RecordMarchingCubes(cmd);
	RecordCounter(cmd);
	RecordSurface(cmd);
}

void FluidLayer::RecordResetCounter(Yoru::RHICommandList& cmd)
{
	if (!m_ArgsBuffer)
		return;

	// Last frame's DrawIndirect may still be reading the args, wait for it before overwriting
	cmd.BufferBarrier(*m_ArgsBuffer, Usage::IndirectRead, Usage::TransferWrite);
	// Only VertexCount (the first 4 bytes), InstanceCount stays 1
	cmd.FillBuffer(*m_ArgsBuffer, 0, 0, sizeof(uint32_t));
	cmd.BufferBarrier(*m_ArgsBuffer, Usage::TransferWrite, Usage::ComputeReadWrite);
}

void FluidLayer::RecordDensityField(Yoru::RHICommandList& cmd)
{
	if (!m_FieldPipeline || !m_GridBuffer)
		return;

	// Write after read: last frame's marching cubes pass read the grid
	cmd.BufferBarrier(*m_GridBuffer, Usage::ComputeRead, Usage::ComputeWrite);

	FieldPushConstants push{};
	push.GridAddress = m_GridBuffer->GetDeviceAddress();
	push.GridSize = m_GridSize;
	push.Time = m_Time;
	push.CellSize = m_CellSize;
	push.Origin = m_GridOrigin;

	cmd.BindPipeline(*m_FieldPipeline);
	cmd.PushConstants(push);
	// One thread per grid sample
	cmd.Dispatch(GroupCount(m_GridSize), GroupCount(m_GridSize), GroupCount(m_GridSize));

	// marching_cubes.comp reads what was just written
	cmd.BufferBarrier(*m_GridBuffer, Usage::ComputeWrite, Usage::ComputeRead);
}

void FluidLayer::RecordMarchingCubes(Yoru::RHICommandList& cmd)
{
	if (!m_MarchingCubesPipeline || !m_GridBuffer || !m_TablesBuffer || !m_VertexBuffer || !m_ArgsBuffer)
		return;

	cmd.BufferBarrier(*m_VertexBuffer, Usage::VertexShaderRead, Usage::ComputeWrite);

	MarchingCubesPushConstants push{};
	push.GridAddress = m_GridBuffer->GetDeviceAddress();
	push.TablesAddress = m_TablesBuffer->GetDeviceAddress();
	push.VerticesAddress = m_VertexBuffer->GetDeviceAddress();
	push.ArgsAddress = m_ArgsBuffer->GetDeviceAddress();
	push.GridSize = m_GridSize;
	push.IsoLevel = m_IsoLevel;
	push.CellSize = m_CellSize;
	push.MaxVertices = m_MaxVertices;
	push.Origin = m_GridOrigin;

	cmd.BindPipeline(*m_MarchingCubesPipeline);
	cmd.PushConstants(push);
	// Because have GridSize-1 cells
	cmd.Dispatch(GroupCount(m_GridSize - 1), GroupCount(m_GridSize - 1), GroupCount(m_GridSize - 1));

	cmd.BufferBarrier(*m_VertexBuffer, Usage::ComputeWrite, Usage::VertexShaderRead);
	// counter.comp reads the final count, back to back dispatches aren't ordered on their own
	cmd.BufferBarrier(*m_ArgsBuffer, Usage::ComputeReadWrite, Usage::ComputeReadWrite);
}

void FluidLayer::RecordCounter(Yoru::RHICommandList& cmd)
{
	uint32_t slot = Yoru::Application::Get()->GetRenderer().GetFrameIndex();
	if (!m_CounterPipeline || !m_ArgsBuffer || slot >= m_CountReadback.size() || !m_CountReadback[slot])
		return;

	CounterPushConstants push{};
	push.ArgsAddress = m_ArgsBuffer->GetDeviceAddress();
	push.ReadbackAddress = m_CountReadback[slot]->GetDeviceAddress();
	push.MaxVertices = m_MaxVertices;

	cmd.BindPipeline(*m_CounterPipeline);
	cmd.PushConstants(push);
	cmd.Dispatch(1);

	// The clamped count feeds DrawIndirect, the unclamped one goes to the CPU after this frame's fence
	cmd.BufferBarrier(*m_ArgsBuffer, Usage::ComputeReadWrite, Usage::IndirectRead);
	cmd.BufferBarrier(*m_CountReadback[slot], Usage::ComputeWrite, Usage::HostRead);
}

void FluidLayer::RecordSurface(Yoru::RHICommandList& cmd)
{
	if (!m_SurfacePipeline || !m_VertexBuffer || !m_ArgsBuffer)
		return;

	auto [width, height] = Yoru::Application::Get()->GetRenderer().GetRenderExtent();
	glm::mat4 projection = glm::perspective(glm::radians(60.0f),
		static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
	projection[1][1] *= -1.0f;	// Vulkan clip space has +Y pointing down

	SurfacePushConstants push{};
	push.ViewProjection = projection * m_Camera.GetViewMatrix();
	push.VerticesAddress = m_VertexBuffer->GetDeviceAddress();
	push.LightDirection = glm::vec4(glm::normalize(m_LightDirection), 1.0f);

	Yoru::RenderingDesc pass{};
	pass.Clear = m_ClearColor;
	cmd.BeginRendering(pass);

	cmd.BindPipeline(*m_SurfacePipeline);
	cmd.PushConstants(push);
	cmd.DrawIndirect(*m_ArgsBuffer);

	cmd.EndRendering();
}

void FluidLayer::OnImGuiRender()
{
	ImGui::Begin("Fluid");
	ImGui::SliderFloat("Iso level", &m_IsoLevel, 0.0f, 1.0f);
	ImGui::Checkbox("Paused", &m_Paused);
	ImGui::ColorEdit3("Clear color", &m_ClearColor.R);
	ImGui::SliderFloat3("Light direction", &m_LightDirection.x, -1.0f, 1.0f);
	// Cell size only goes into push constants, so it applies next frame with no reallocation
	ImGui::SliderFloat("Cell size", &m_CellSize, 0.01f, 0.2f, "%.4f");

	// Applied on release only: every change reallocates the grid and stalls the GPU
	ImGui::SliderInt("Grid size", &m_GridSizeSlider, 8, 128);
	if (ImGui::IsItemDeactivatedAfterEdit())
		m_PendingGridSize = static_cast<uint32_t>(m_GridSizeSlider);

	ImGui::Text("Grid: %u^3, box %.2f units, max %u vertices", m_GridSize,
		static_cast<float>(m_GridSize - 1) * m_CellSize, m_MaxVertices);

	// Read back a few frames late, the GPU count is never waited on
	ImGui::Text("Triangles: %u (%u vertices)", m_LastVertexCount / 3, m_LastVertexCount);
	if (m_LastVertexCount > m_MaxVertices)
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "Vertex budget exceeded, %u triangles dropped",
			(m_LastVertexCount - m_MaxVertices) / 3);
	ImGui::DragFloat3("Origin", &m_GridOrigin.x, 1.0f, -9999.0f, 9999.0f);

	ImGui::TextDisabled("M toggles mouse look, WASD/QE moves");
	ImGui::End();
}
