#pragma once

#include <memory>
#include <string>

#include "Yoru/Core/BackEndWindow.h"
#include "Yoru/Renderer/RendererAPI.h"
#include "Yoru/Core/Input.h"
#include "Yoru/Core/Layer.h"
#include "Yoru/Core/LayerStack.h"
#include "Yoru/Events/ApplicationEvent.h"
#include "Yoru/Events/Event.h"
#include "Platform/Vulkan/VKRenderer.h"

namespace Yoru
{
	class ImGuiLayer;
	struct ApplicationSpecifications
	{
		std::string Name = "Engine";
		std::string LogPath = "";
		std::pair<uint32_t, uint32_t> Resolution = { 1920, 1080 };
	};

	class Application
	{
	public:
		Application() = delete;
		Application(ApplicationSpecifications& AppSpec);
		~Application();

		void OnEvent(Event& e);
		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		void Run();
		void Shutdown() { m_Running = false; }
		static Application* const Get() { return s_Application; }
		BackEndWindow* const GetWindow() { return &m_Window; }
		const std::string& GetAppName() { return AppSpecs.Name; }
		
		VKRenderer* const GetVulkanRenderer();
		ImGuiLayer* const GetImGuiLayer() { return m_ImGuiLayer; }
		LayerStack& GetLayerStack() { return m_LayerStack; }

	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

	public:
		ApplicationSpecifications AppSpecs;

	private:
		BackEndWindow m_Window;

		bool m_Running = true;
		bool m_Minimized = false;
		std::unique_ptr<RendererAPI> m_Renderer;
		LayerStack m_LayerStack;
		ImGuiLayer* m_ImGuiLayer = nullptr;
		std::chrono::system_clock::time_point m_LastFrameTime = {};

		static Application* s_Application;
	};

	// Needs to be defined in ClientApp
	Application* CreateApplication(ApplicationSpecifications& args);
}