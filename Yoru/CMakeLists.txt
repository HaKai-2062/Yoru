cmake_minimum_required(VERSION 3.30)
project(Yoru)

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

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS 
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
file(GLOB_RECURSE GLSL_SOURCE_FILES CONFIGURE_DEPENDS 
    "${CMAKE_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders/*.comp"
    )
file(GLOB_RECURSE GLSL_EXTRA_FILES CONFIGURE_DEPENDS 
    "${CMAKE_SOURCE_DIR}/shaders/*.glsl"
)

add_library("${PROJECT_NAME}" STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS} ${GLSL_SOURCE_FILES} ${GLSL_EXTRA_FILES})
source_group("Shader Files" FILES ${GLSL_SOURCE_FILES} ${GLSL_EXTRA_FILES})
#source_group("Source Files" FILES ${ENGINE_SOURCES})
#source_group("Header Files" FILES ${ENGINE_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS})
set_property(TARGET "${PROJECT_NAME}" PROPERTY CXX_STANDARD 20)

#if(MSVC) # If using the VS compiler...

	#set_target_properties(${BUILD_TARGET} PROPERTIES LINK_FLAGS "/PROFILE") # for profiling

	#target_compile_definitions("${PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)		# secure function warnings
	#set_target_properties("${PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup") #no console
	#set_property(TARGET "${PROJECT_NAME}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug<$<CONFIG:Debug>:Debug>")

#endif()

find_package(Vulkan REQUIRED)
find_program(GLSL_VALIDATOR glslangValidator REQUIRED HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

target_compile_definitions("${PROJECT_NAME}" PRIVATE 
	GLFW_INCLUDE_NONE=1
	GLM_FORCE_RADIANS=1
	GLM_FORCE_DEPTH_ZERO_TO_ONE=1
	GLM_ENABLE_EXPERIMENTAL=1
	VK_PROTOTYPES=1
	YORU_VALIDATION_LAYERS=1
	SHADER_PATH="${CMAKE_SOURCE_DIR}/shaders/spirv/"
	ASSET_PATH="${CMAKE_SOURCE_DIR}/assets/")

target_precompile_headers("${PROJECT_NAME}" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h")

target_include_directories("${PROJECT_NAME}" PUBLIC 
	"${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm/"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/")
#	"${CMAKE_SOURCE_DIR}/vendor/GLFW/include/"
#	"${CMAKE_SOURCE_DIR}/vendor/imgui/"
#	"${CMAKE_SOURCE_DIR}/vendor/imgui/backends/"
#	"${CMAKE_SOURCE_DIR}/vendor/stb/"
#	"${CMAKE_SOURCE_DIR}/vendor/tinyobjloader/"
#	"${CMAKE_SOURCE_DIR}/vendor/vk-bootstrap/"
#	"${CMAKE_SOURCE_DIR}/vendor/VulkanMemoryAllocator/"
#	"${CMAKE_SOURCE_DIR}/vendor/libktx/include/"
#	"${CMAKE_SOURCE_DIR}/vendor/libktx/other_include/"
#	"${Vulkan_INCLUDE_DIRS}")

target_link_libraries("${PROJECT_NAME}" PUBLIC 
	glfw
	glad
	glm
	imgui
	tinyobjloader
	vk-bootstrap
	VulkanMemoryAllocator
	stb
	fmt
	fastgltf
	libktx
	${Vulkan_LIBRARIES})

# Compile all shaders
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${CMAKE_SOURCE_DIR}/shaders/spirv/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  message(STATUS COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${PROJECT_NAME} Shaders)

if (TARGET Shaders)
	set_target_properties(Shaders PROPERTIES FOLDER "_Hidden")
endif()