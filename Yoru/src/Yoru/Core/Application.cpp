#include "Yoru/Core/Application.h"
#include "Yoru/Core/Log.h"
#include "Yoru/Events/ApplicationEvent.h"
#include "Yoru/Renderer/RendererAPI.h"
#include "Yoru/UI/ImGuiLayer.h"

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
		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);
	}

	Application::~Application()
	{
		m_LayerStack.ClearAllLayers();
		m_Renderer->Shutdown();
		Log::Shutdown();
		m_Window.Shutdown();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::OnWindowResize));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.Handled)
				break;
			(*it)->OnEvent(e);
		}
	}

	VKRenderer* const Application::GetVulkanRenderer()
	{
		if (RendererAPI::s_API == RendererAPI::API::Vulkan)
			return dynamic_cast<VKRenderer*>(m_Renderer.get());

		return nullptr;
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}

		m_Minimized = false;
		//Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());

		return false;
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::Run()
	{
		while (m_Running)
		{
			auto start = std::chrono::system_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(m_LastFrameTime - start);
			m_LastFrameTime = start;

			// BeginFrames
			GetWindow()->BeginFrame();
			Input::BeginFrame();
			if (Input::IsKeyPressed(Yoru::Key::Escape) || GetWindow()->ShouldWindowClose())
			{
				GetWindow()->CloseWindow();
				m_Running = false;
				continue;
			}
			if (Input::IsKeyReleased(Yoru::Key::M))
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

			for (Layer* layer : m_LayerStack)
				layer->OnUpdate(elapsed.count() * 1e6);

			m_Renderer->Update();

			//m_ImGuiLayer->BeginFrame();
			//for (Layer* layer : m_LayerStack)
			//	layer->OnImGuiRender();
			//m_ImGuiLayer->EndFrame();

			// EndFrames
			Input::EndFrame();
		}
	}
}