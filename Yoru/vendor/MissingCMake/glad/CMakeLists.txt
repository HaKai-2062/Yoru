cmake_minimum_required(VERSION 3.15)
project(glad)

add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../glad/src/glad.c")

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../glad/include/")
