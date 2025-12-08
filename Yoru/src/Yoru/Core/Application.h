#pragma once

#include <memory>
#include <string>

#include "Yoru/Core/BackEndWindow.h"
#include "Yoru/Renderer/RendererAPI.h"
#include "Input.h"

namespace Yoru
{
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

		void Run();
		void Shutdown() { m_Running = false; }
		static Application* const Get() { return s_Application; }
		BackEndWindow* const GetWindow() { return &m_Window; }
		
	private:
		BackEndWindow m_Window;

		bool m_Running = true;
		bool m_Minimized = false;
		std::unique_ptr<RendererAPI> m_Renderer;

		static Application* s_Application;
	};
}