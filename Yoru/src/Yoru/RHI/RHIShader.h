#pragma once

#include "Yoru/RHI/RHICommon.h"

namespace Yoru
{
	class RHIShader
	{
	public:
		virtual ~RHIShader() = default;
		virtual ShaderStage GetStage() const = 0;
	};
}