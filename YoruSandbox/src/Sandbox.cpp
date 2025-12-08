#include <memory>

#include "Yoru/Core/Application.h"
#include "Yoru/Core/Log.h"

int main(int argc, char** argv)
{
	Yoru::ApplicationSpecifications AppSpecs;
	AppSpecs.Name = "Yoru";
	AppSpecs.LogPath = "";
	AppSpecs.Resolution = { 1920, 1080 };

	std::unique_ptr<Yoru::Application> app = std::make_unique<Yoru::Application>(AppSpecs);
	app->Run();

	return 0;
}
