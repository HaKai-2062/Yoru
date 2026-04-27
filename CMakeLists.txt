cmake_minimum_required (VERSION 3.15)
project (YoruProject)

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

#if (MSVC)
#    add_compile_options(/EHsc)
#endif()
#set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_PROTOTYPES")
#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES")
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) #link time optimization


option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(GLFW_DOCUMENT_INTERNALS "Include internals in documentation" OFF)

set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(Yoru/vendor/simdjson)
add_subdirectory(Yoru/vendor/fastgltf)
add_subdirectory(Yoru/vendor/fmt)
add_subdirectory(Yoru/vendor/GLFW)
add_subdirectory(Yoru/vendor/glm)
add_subdirectory(Yoru/vendor/MissingCMake/imgui)
add_subdirectory(Yoru/vendor/MissingCMake/glad)
add_subdirectory(Yoru/vendor/MissingCMake/stb)
add_subdirectory(Yoru/vendor/libktx)
add_subdirectory(Yoru/vendor/tinyobjloader)
add_subdirectory(Yoru/vendor/vk-bootstrap)
add_subdirectory(Yoru/vendor/VulkanMemoryAllocator)

set_target_properties(simdjson fastgltf fmt glfw glm imgui glad stb libktx tinyobjloader vk-bootstrap VulkanMemoryAllocator
    PROPERTIES FOLDER "Dependencies"
)
if(TARGET uninstall)
    set_target_properties(uninstall PROPERTIES FOLDER "_Hidden")
endif()

if (TARGET update_mappings)
	set_target_properties(update_mappings PROPERTIES FOLDER "_Hidden")
endif()

add_subdirectory(Yoru)
add_subdirectory(YoruSandbox)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT YoruSandbox)
