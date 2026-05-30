#include "Yoru/Core/Application.h"
#include "Yoru/Core/Input.h"
#include "Yoru/Core/Log.h"

#include <GLFW/glfw3.h>

namespace Yoru
{
	std::pair<double, double> Input::m_MousePos = std::make_pair(0.0, 0.0);
	std::pair<double, double> Input::m_LastMousePos = std::make_pair(0.0, 0.0);
	std::array<bool, Yoru::Key::Last> Input::m_PreviousKeyState{};
	std::array<bool, Yoru::Key::Last> Input::m_CurrentKeyState{};

	void Input::SetCursorState(CursorState cursorState)
	{
		GLFWwindow* const window = Application::Get()->GetWindow()->GetNativeWindow();
		int32_t state = GLFW_CURSOR_HIDDEN;
		if (cursorState == CursorState::Disabled)
			state = GLFW_CURSOR_DISABLED;
		else if (cursorState == CursorState::Show)
			state = GLFW_CURSOR_NORMAL;

		glfwSetInputMode(window, GLFW_CURSOR, state);
	}

	CursorState Input::GetCursorState()
	{
		GLFWwindow* const window = Application::Get()->GetWindow()->GetNativeWindow();
		if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
			return CursorState::Disabled;
		else if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL)
			return CursorState::Show;

		return CursorState::Hidden;
	}

	bool Input::IsKeyPressed(uint32_t key)
	{
		return m_CurrentKeyState[key];
	}

	bool Input::IsKeyReleased(uint32_t key)
	{
		return m_PreviousKeyState[key] && !m_CurrentKeyState[key];
	}

	void Input::BeginFrame()
	{
		GLFWwindow* const window = Application::Get()->GetWindow()->GetNativeWindow();
		for (int key = 0; key < Yoru::Key::Last; key++)
		{
			m_CurrentKeyState[key] = (glfwGetKey(window, key) == GLFW_PRESS);
		}

		// Mouse events
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		m_MousePos = std::make_pair(x, y);
	}

	void Input::EndFrame()
	{
		m_PreviousKeyState = m_CurrentKeyState;
		m_LastMousePos = m_MousePos;
	}
}