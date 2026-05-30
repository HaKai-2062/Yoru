#pragma once

#include <Yoru/Core/Layer.h>

class Viewport : public Yoru::Layer
{
public:
	Viewport() = default;
	virtual ~Viewport() = default;

	virtual void OnAttach() override {};
	virtual void OnDetach() override {};
	virtual void OnUpdate(float deltaTime) override {};
	void OnEvent(Yoru::Event& e) override {};

	virtual void OnImGuiRender() override;
};