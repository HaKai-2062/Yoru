#pragma once

#include "Yoru/Core/Layer.h"

struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkQueue_T;
struct VkDescriptorPool_T;
struct VkCommandBuffer_T;
using VkInstance = VkInstance_T*;
using VkPhysicalDevice = VkPhysicalDevice_T*;
using VkDevice = VkDevice_T*;
using VkQueue = VkQueue_T*;
using VkDescriptorPool = VkDescriptorPool_T*;
using VkCommandBuffer = VkCommandBuffer_T*;
enum VkFormat;
class UIPanel;

namespace Yoru
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void BeginFrame();
		void EndFrame();

		void BlockEvents(bool block) { m_BlockEvents = block; }
		uint32_t GetActiveWidgetID() const;
	private:
		bool m_BlockEvents = true;
		VkDescriptorPool m_ImGuiPool = {};
	};
}
