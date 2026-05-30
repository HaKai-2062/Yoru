#pragma once

#include <memory>

#include "Yoru/Core/Application.h"
#include "Yoru/Core/Base.h"

extern Yoru::Application* Yoru::CreateApplication(ApplicationSpecifications& appSpecs);

int main(int argc, char** argv)
{
	// Can be overrriden by client in CreateApplication
	Yoru::ApplicationSpecifications appSpecs;
	appSpecs.Name = "Yoru Engine";
	appSpecs.LogPath = "";
	appSpecs.Resolution = { 1920, 1080 };

	// We dont need make_unique bcz it is not new construction but ownership
	std::unique_ptr<Yoru::Application> app(Yoru::CreateApplication(appSpecs));
	app->AppSpecs = appSpecs;
	if (app)
		app->Run();

	return 0;
}