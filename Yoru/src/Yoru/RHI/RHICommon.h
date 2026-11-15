#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace Yoru
{
	class RHIShader;

	enum class ShaderStage
	{
		Vertex,
		Fragment,
		Compute
	};

	enum class IndexType { UInt16, UInt32 };
	enum class PipelineType { Graphics, Compute };
	enum class PrimitiveTopology { TriangleList, LineList, PointList };
	enum class CullMode { None, Front, Back };
	enum class FrontFace { Clockwise, CounterClockwise };

	enum class BufferUsage : uint32_t
	{
		None	 = 0,
		Vertex	 = 1 << 0,	// read through buffer device address (vertex pulling)
		Index	 = 1 << 1,
		Storage	 = 1 << 2,
		Indirect = 1 << 3,
		Uniform	 = 1 << 4
	};

	inline BufferUsage operator|(BufferUsage a, BufferUsage b) { return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }
	inline bool HasFlag(BufferUsage value, BufferUsage flag) { return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0; }

	enum class MemoryLocation
	{
		GPUOnly,	// device local, InitialData goes through staging
		CPUToGPU,	// persistently mapped, written every frame
		GPUToCPU	// readback
	};

	struct Viewport
	{
		float X = 0.0f, Y = 0.0f;
		float Width = 0.0f, Height = 0.0f;
		float MinDepth = 0.0f, MaxDepth = 1.0f;
	};

	struct Rect2D
	{
		int32_t X = 0, Y = 0;
		uint32_t Width = 0, Height = 0;
	};

	struct ClearColor
	{
		float R = 0.0f, G = 0.0f, B = 0.0f, A = 1.0f;
	};

	struct InstanceDesc
	{
		bool EnableValidationLayers = false;
		void* NativeWindow = nullptr;
	};

	struct DeviceDesc
	{
		bool PreferDiscreteGPU = true;
	};

	struct SwapchainDesc
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		bool VSync = false;
	};

	struct RendererDesc
	{
		InstanceDesc InstDesc;
		DeviceDesc DeviceDesc;
		SwapchainDesc SwapchainDesc;
	};

	struct ShaderDesc
	{
		ShaderStage Stage;
		std::string Path;
		std::string EntryPoint = "main";
	};

	struct BufferDesc
	{
		uint64_t Size = 0;
		BufferUsage Usage = BufferUsage::None;
		MemoryLocation Memory = MemoryLocation::GPUOnly;
		const void* InitialData = nullptr;
	};

	// Color/depth formats come from the renderer's targets
	struct GraphicsPipelineDesc
	{
		RHIShader* VertexShader = nullptr;
		RHIShader* FragmentShader = nullptr;
		PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
		CullMode Cull = CullMode::None;
		FrontFace Front = FrontFace::CounterClockwise;
		// Must match RenderingDesc::UseDepth of the passes this pipeline is drawn in
		bool UseDepthTarget = true;
		bool DepthTest = true;
		bool DepthWrite = true;
		uint32_t PushConstantSize = 0;	// visible to vertex + fragment
	};

	// Only the swapchain (+ renderer owned depth) can be targeted for now
	struct RenderingDesc
	{
		bool ClearColorTarget = true;	// false loads the previous contents
		ClearColor Clear = {};
		bool UseDepth = true;
		bool ClearDepth = true;
	};
}
