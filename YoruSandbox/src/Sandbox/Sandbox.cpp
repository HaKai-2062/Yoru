#include <Yoru/Core/EntryPoint.h>
#include <Yoru/Core/Application.h>

#include "Viewport.h"
#include "UIPanel.h"

class Sandbox : public Yoru::Application
{
public:
	Sandbox(Yoru::ApplicationSpecifications& appSpec)
		: Yoru::Application(appSpec)
	{
		PushLayer(new Viewport());
		PushLayer(new UIPanel());
	}

	~Sandbox() = default;
};

Yoru::Application* Yoru::CreateApplication(ApplicationSpecifications& appSpecs)
{
	appSpecs.Name = "Yoru Sandbox";
	appSpecs.LogPath = "";
	appSpecs.Resolution = { 1920, 1080 };

	return new Sandbox(appSpecs);
}