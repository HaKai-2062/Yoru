#pragma once

#include <Yoru/Core/Layer.h>
#include <Yoru/RHI/RHICommon.h>
#include <Yoru/Scene/Camera.h>

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace Yoru
{
	class RHIShader;
	class RHIPipeline;
	class RHIBuffer;
}

// Every push constant struct below must match its push_constant block in the shader byte for byte.
// Keep each one <= 128 bytes (the minimum every GPU supports). Buffer addresses are 8 bytes and need
// 8 byte alignment, so put them first. If you outgrow 128 bytes, move the parameters into a CPUToGPU
// buffer (one per frame in flight, see RHIRenderer::GetFrameIndex) and push only its address.

// shaders/field.comp
struct FieldPushConstants
{
	uint64_t GridAddress;
	uint32_t GridSize;		// samples per axis, the grid stores GridSize^3 floats
	float Time;
	glm::vec3 Origin;
	float CellSize;
};

// shaders/marching_cubes.comp
struct MarchingCubesPushConstants
{
	uint64_t GridAddress;
	uint64_t TablesAddress;		// edge table followed by triangle table
	uint64_t VerticesAddress;
	uint64_t ArgsAddress;		// Yoru::DrawIndirectArgs, VertexCount doubles as the triangle counter
	uint32_t GridSize;
	float IsoLevel;
	float CellSize;				// world space distance between two samples
	uint32_t MaxVertices;		// the shader must never write past this
	glm::vec3 Origin;
};

// shaders/counter.comp
struct CounterPushConstants
{
	uint64_t ArgsAddress;
	uint64_t ReadbackAddress;	// this frame's GPUToCPU slot
	uint32_t MaxVertices;
};

// shaders/surface.vert / surface.frag
struct SurfacePushConstants
{
	glm::mat4 ViewProjection;
	uint64_t VerticesAddress;
	uint64_t Padding;
	glm::vec4 LightDirection;	// world space, the surface is generated in world space
};

// Marching cubes writes this instead of Yoru::Vertex. Two vec4s keep std430 simple (a vec3 would
// be padded to 16 bytes anyway) and it's 32 bytes instead of the 48 byte mesh vertex.
struct SurfaceVertex
{
	glm::vec4 Position;	// w unused
	glm::vec4 Normal;	// w unused
};

class FluidLayer : public Yoru::Layer
{
public:
	FluidLayer();
	virtual ~FluidLayer() override;

	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate(float deltaTime) override;
	virtual void OnRender(Yoru::RHICommandList& cmd) override;
	virtual void OnImGuiRender() override;

private:
	bool CreatePipelines();
	bool CreateBuffers();

	// One function per GPU pass keeps OnRender readable and makes passes easy to toggle while debugging
	void RecordResetCounter(Yoru::RHICommandList& cmd);
	void RecordDensityField(Yoru::RHICommandList& cmd);
	void RecordMarchingCubes(Yoru::RHICommandList& cmd);
	void RecordCounter(Yoru::RHICommandList& cmd);
	void RecordSurface(Yoru::RHICommandList& cmd);

private:
	std::unique_ptr<Yoru::RHIShader> m_FieldShader;
	std::unique_ptr<Yoru::RHIShader> m_MarchingCubesShader;
	std::unique_ptr<Yoru::RHIShader> m_CounterShader;
	std::unique_ptr<Yoru::RHIShader> m_SurfaceVertexShader;
	std::unique_ptr<Yoru::RHIShader> m_SurfaceFragmentShader;
	std::unique_ptr<Yoru::RHIPipeline> m_FieldPipeline;
	std::unique_ptr<Yoru::RHIPipeline> m_MarchingCubesPipeline;
	std::unique_ptr<Yoru::RHIPipeline> m_CounterPipeline;
	std::unique_ptr<Yoru::RHIPipeline> m_SurfacePipeline;

	// GPU only state, one copy each: barriers order the frames on the single queue
	std::unique_ptr<Yoru::RHIBuffer> m_GridBuffer;		// float[GridSize^3]
	std::unique_ptr<Yoru::RHIBuffer> m_TablesBuffer;	// marching cubes lookup tables, uploaded once
	std::unique_ptr<Yoru::RHIBuffer> m_VertexBuffer;	// SurfaceVertex[MaxVertices]
	std::unique_ptr<Yoru::RHIBuffer> m_ArgsBuffer;		// Yoru::DrawIndirectArgs

	// One uint per frame in flight: the CPU reads a slot only after BeginFrame waited on that frame's fence
	std::vector<std::unique_ptr<Yoru::RHIBuffer>> m_CountReadback;
	uint32_t m_LastVertexCount = 0;	// unclamped, a few frames old

	uint32_t m_GridSize = 64;
	uint32_t m_PendingGridSize = m_GridSize;	// set by the ImGui slider, applied in OnUpdate before recording
	int m_GridSizeSlider = static_cast<int>(m_GridSize);	// what the slider shows while dragging
	uint32_t m_MaxVertices = 2'000'000;
	float m_CellSize = 4.0f / 63.0f;	// the grid spans 4 world units
	float m_IsoLevel = 0.5f;
	float m_Time = 0.0f;
	bool m_Paused = false;

	Yoru::Camera m_Camera{ glm::vec3(0.0f, 0.5f, 6.0f) };
	Yoru::ClearColor m_ClearColor{ 0.05f, 0.05f, 0.08f, 1.0f };
	glm::vec3 m_LightDirection = glm::vec3(-0.4f, -1.0f, -0.6f);
	glm::vec3 m_GridOrigin = glm::vec3(0.0f, 0.0f, 0.0f);
};
