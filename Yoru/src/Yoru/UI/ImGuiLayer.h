#pragma once

#include "Yoru/Core/Layer.h"

namespace Yoru
{
	class RHICommandList;

	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void BeginFrame();
		void EndFrame(RHICommandList& cmd);

		void BlockEvents(bool block) { m_BlockEvents = block; }
		uint32_t GetActiveWidgetID() const;
	private:
		bool m_BlockEvents = true;
	};
}
