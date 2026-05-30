#include "Viewport.h"

#include <imgui.h>

void Viewport::OnImGuiRender()
{
	// Dockspace
	ImGuiDockNodeFlags flags;
	flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpaceOverViewport(0, nullptr, flags);

	// Render into viewport
	//ImGui::Begin("MainViewport");

	//ImVec2 viewportSize = ImGui::GetContentRegionAvail();
	//ImGui::Image((ImTextureID)GetViewportTexture(), viewportSize);
	//ImGui::End();
}

