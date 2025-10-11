#pragma once

#include <utility>
#include <array>

#include "Yoru/Core/Keycodes.h"

namespace Yoru
{
	enum class CursorState : uint8_t
	{
		Disabled,
		Show,
		Hidden,
	};

	class Input
	{
	public:
		static void SetCursorState(CursorState cursorState);
		static CursorState GetCursorState();

		static void SetMousePositions(double x, double y) { m_MousePos = std::make_pair(x, y); }
		static std::pair<double, double> GetMousePosition() { return std::make_pair(m_MousePos.first, m_MousePos.second); }
		static std::pair<double, double> GetDeltaMousePosition() { return std::make_pair(m_MousePos.first - m_LastMousePos.first, m_LastMousePos.second - m_MousePos.second); }
		static bool IsKeyPressed(uint32_t key);
		static bool IsKeyReleased(uint32_t key);

		static void BeginFrame();
		static void EndFrame();

	private:
		static std::pair<double, double> m_MousePos;
		static std::pair<double, double> m_LastMousePos;
		static std::array<bool, YORU_KEY_LAST> m_PreviousKeyState;
		static std::array<bool, YORU_KEY_LAST> m_CurrentKeyState;
	};
}