#pragma once

#include "Yoru/RHI/RHICommon.h"

namespace Yoru
{
	class RHIBuffer
	{
	public:
		virtual ~RHIBuffer() = default;

		virtual uint64_t GetSize() const = 0;
		virtual BufferUsage GetUsage() const = 0;
		// 0 for buffers that can't be addressed from shaders (e.g. index only)
		virtual uint64_t GetDeviceAddress() const = 0;
		// nullptr unless memory is CPUToGPU / GPUToCPU
		virtual void* GetMappedData() const = 0;
	};
}
