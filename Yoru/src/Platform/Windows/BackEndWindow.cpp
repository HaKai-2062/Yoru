#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Yoru/Core/BackEndWindow.h"
#include "Yoru/Core/Log.h"

namespace Yoru
{
	bool BackEndWindow::Startup(const char* windowTitle, const std::pair<uint32_t, uint32_t>& extent)
	{
		m_WindowExtent = extent;

		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		m_Window = glfwCreateWindow(m_WindowExtent.first, m_WindowExtent.second, windowTitle, nullptr, nullptr);
	
		return true;
	}

	bool BackEndWindow::Shutdown()
	{
		glfwTerminate();

		return true;
	}

	void BackEndWindow::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
	{
		if (glfwCreateWindowSurface(instance, m_Window, nullptr, surface) != VK_SUCCESS)
		{
			Log::Write(LogLevel::FATAL, "Failed to create window surface!");
		}
	}

	std::pair<uint32_t, uint32_t> BackEndWindow::GetWindowSize()
	{
		int x, y;
		glfwGetWindowSize(m_Window, &x, &y);
		m_WindowExtent = std::make_pair(x, y);
		return m_WindowExtent;
	}

	void BackEndWindow::SetWindowTitle(const char* title) { glfwSetWindowTitle(m_Window, title); }
	void BackEndWindow::BeginFrame() { glfwPollEvents(); }
	void BackEndWindow::CloseWindow() { glfwSetWindowShouldClose(m_Window, GLFW_TRUE); }
	void BackEndWindow::DestroyWindow() { glfwDestroyWindow(m_Window); }
	bool BackEndWindow::ShouldWindowClose() { return glfwWindowShouldClose(m_Window); }
	bool BackEndWindow::IsWindowMinimized() { return glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED); }

	double BackEndWindow::GetTime() { return glfwGetTime(); }
}