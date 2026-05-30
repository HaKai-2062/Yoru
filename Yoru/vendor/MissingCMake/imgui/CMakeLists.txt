cmake_minimum_required(VERSION 3.15)
project(imgui)

find_package(Vulkan REQUIRED)

add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE 
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/imgui.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/imgui_demo.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/imgui_draw.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/imgui_tables.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/imgui_widgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/backends/imgui_impl_glfw.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/backends/imgui_impl_vulkan.cpp"

# Optional: headers for IDE visibility
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/imgui.h"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/imconfig.h"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/imgui_internal.h"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/backends/imgui_impl_glfw.h"
"${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/backends/imgui_impl_vulkan.h"
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/" "${CMAKE_CURRENT_SOURCE_DIR}/../../imgui/backends" "${Vulkan_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)