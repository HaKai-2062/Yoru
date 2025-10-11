#pragma once

#include <utility>
#include <stdint.h>
#include <memory>

struct GLFWwindow;

struct VkInstance_T;
struct VkSurfaceKHR_T;
using VkInstance = VkInstance_T*;
using VkSurfaceKHR = VkSurfaceKHR_T*;

namespace Yoru
{
    class BackEndWindow
    {
    public:
        BackEndWindow(const BackEndWindow&) = delete;
        BackEndWindow& operator=(const BackEndWindow&) = delete;
        BackEndWindow(BackEndWindow&&) = delete;
        BackEndWindow& operator=(BackEndWindow&&) = delete;

        BackEndWindow() = default;
        ~BackEndWindow() = default;

        bool Startup(const char* windowTitle = "Engine", const std::pair<uint32_t, uint32_t>& extent = { 1920, 1080 });
        bool Shutdown();

        void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
        std::pair<uint32_t, uint32_t> GetWindowSize();
        void SetWindowTitle(const char* title);
        void BeginFrame();

        GLFWwindow* const GetNativeWindow() { return m_Window; }
        void CloseWindow();
        void DestroyWindow();
        bool ShouldWindowClose();
        bool IsWindowMinimized();

        // TDL: Use a timer class later on
        static double GetTime();

    private:
        std::pair<uint32_t, uint32_t> m_WindowExtent = { 1920, 1080 };
        GLFWwindow* m_Window = nullptr;
    };
}