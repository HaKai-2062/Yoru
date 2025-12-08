cmake_minimum_required(VERSION 3.15)
project(stb)

# stb is header-only, so use INTERFACE
add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../../stb")
