#include "Yoru/Core/Application.h"
#include "Yoru/Core/Log.h"

#include <chrono>
#include <thread>

namespace Yoru
{
	Application* Application::s_Application = nullptr;

	Application::Application(ApplicationSpecifications& AppSpec)
	{
		if (s_Application)
		{
			Log::Write(LogLevel::ERROR, "Application alreading running!");
			return;
		}

		s_Application = this;
		Log::Startup(AppSpec.LogPath.c_str());
		m_Window.Startup(AppSpec.Name.c_str(), AppSpec.Resolution);
		// InitRenderer
		// Init ImGui
	}

	Application::~Application()
	{
		// ShutdownRenderer
		Log::Shutdown();
		m_Window.Shutdown();
	}

	void Application::Run()
	{
		while (m_Running)
		{
			// BeginFrames
			GetWindow()->BeginFrame();
			Input::BeginFrame();
			if (Input::IsKeyPressed(YORU_KEY_ESCAPE))
			{
				GetWindow()->CloseWindow();
				m_Running = false;
			}
			if (Input::IsKeyReleased(YORU_KEY_M))
			{
				if (Input::GetCursorState() == CursorState::Disabled)
					Input::SetCursorState(CursorState::Show);
				else
					Input::SetCursorState(CursorState::Disabled);
			}



			// EndFrames
			Input::EndFrame();
		}
	}
}