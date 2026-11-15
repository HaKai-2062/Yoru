cmake_minimum_required(VERSION 3.30)
project(YoruSandbox)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
  "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,\
$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,\
$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "x64 Debug")
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
else()
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE SANDBOX_HEADERS CONFIGURE_DEPENDS 
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_executable(${PROJECT_NAME} ${SANDBOX_SOURCES} ${SANDBOX_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${SANDBOX_SOURCES} ${SANDBOX_HEADERS})
set_property(TARGET "${PROJECT_NAME}" PROPERTY CXX_STANDARD 20)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_SOURCE_DIR}/Yoru/src")

target_link_libraries(${PROJECT_NAME} PRIVATE 
	Yoru)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	SANDBOX_SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/shaders/spirv/"
	SANDBOX_ASSET_PATH="${CMAKE_SOURCE_DIR}/assets/")

# Sandbox owns its shaders, compiled the same way as the engine's
file(GLOB SANDBOX_SHADER_FILES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
	"${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
	"${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp")
target_sources(${PROJECT_NAME} PRIVATE ${SANDBOX_SHADER_FILES})
source_group("Shader Files" FILES ${SANDBOX_SHADER_FILES})

foreach(GLSL ${SANDBOX_SHADER_FILES})
	get_filename_component(FILE_NAME ${GLSL} NAME)
	set(SPIRV "${CMAKE_CURRENT_SOURCE_DIR}/shaders/spirv/${FILE_NAME}.spv")
	add_custom_command(
		OUTPUT ${SPIRV}
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders/spirv"
		COMMAND ${GLSL_VALIDATOR} -V --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
		DEPENDS ${GLSL})
	list(APPEND SANDBOX_SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(SandboxShaders DEPENDS ${SANDBOX_SPIRV_FILES})
set_target_properties(SandboxShaders PROPERTIES FOLDER "_Hidden")
add_dependencies(${PROJECT_NAME} SandboxShaders)