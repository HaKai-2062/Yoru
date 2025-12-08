#include "Yoru/Core/Application.h"
#include "Yoru/Renderer/RendererAPI.h"
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
		m_Renderer = RendererAPI::Create(RendererAPI::API::Vulkan);
		// Init ImGui
	}

	Application::~Application()
	{
		m_Renderer->Shutdown();
		Log::Shutdown();
		m_Window.Shutdown();
	}

	void Application::Run()
	{
		while (m_Running)
		{
			auto start = std::chrono::system_clock::now();

			// BeginFrames
			GetWindow()->BeginFrame();
			Input::BeginFrame();
			if (Input::IsKeyPressed(YORU_KEY_ESCAPE) || GetWindow()->ShouldWindowClose())
			{
				GetWindow()->CloseWindow();
				m_Running = false;
				continue;
			}
			if (Input::IsKeyReleased(YORU_KEY_M))
			{
				if (Input::GetCursorState() == CursorState::Disabled)
					Input::SetCursorState(CursorState::Show);
				else
					Input::SetCursorState(CursorState::Disabled);
			}
			if (GetWindow()->IsWindowMinimized())
			{
				// Skip rendering
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			//m_Camera.ProcessKeyEvents(m_DeltaTime);
			//m_Camera.ProcessMouseEvents(m_DeltaTime);

			m_Renderer->Update();

			// EndFrames
			Input::EndFrame();

			auto end = std::chrono::system_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			//m_Renderer->Stats.FrameTime = elapsed.count() / 1000.f;
		}
	}
}