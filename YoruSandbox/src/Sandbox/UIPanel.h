#pragma once

#include <Yoru/Core/Layer.h>

class UIPanel : public Yoru::Layer
{
public:
	UIPanel() = default;
	virtual ~UIPanel() = default;

	virtual void OnAttach() override {};
	virtual void OnDetach() override {};
	virtual void OnUpdate(float deltaTime) override {};
	void OnEvent(Yoru::Event& e) override {};

	virtual void OnImGuiRender() override;
};