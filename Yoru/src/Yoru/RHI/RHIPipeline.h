#pragma once

#include "Yoru/RHI/RHICommon.h"

namespace Yoru
{
	class RHIPipeline
	{
	public:
		virtual ~RHIPipeline() = default;

		virtual PipelineType GetType() const = 0;
		virtual uint32_t GetPushConstantSize() const = 0;
	};
}
