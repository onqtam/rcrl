cmake_minimum_required(VERSION 3.0.0)

project(RCRL)

####################################################################################################
# global flags and directories setup
####################################################################################################

# precompiled header macro - taken (and slightly modified - removed "-std=..." stuff) from here: https://github.com/iboB/cmake-pch
include(src/third_party/precompiled_header.cmake)

# directories - everything goes in the same place
set(OUTPUT_DIR ${PROJECT_BINARY_DIR}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# latest c++ standards
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -fvisibility=hidden")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
endif()

# the file that will 
set(plugin_file ${PROJECT_BINARY_DIR}/plugin.cpp)
# touch the file so it exists
file(WRITE ${plugin_file} "")

####################################################################################################
# the main executable
####################################################################################################

add_executable(host_app
# host app sources
    src/main.cpp
    src/host_app.cpp
    src/host_app.h
# RCRL sources
    src/rcrl/rcrl.h
    src/rcrl/rcrl.cpp
    src/rcrl/rcrl_parser.h
    src/rcrl/rcrl_parser.cpp
    src/rcrl/rcrl_for_plugin.h
# imgui integration
    src/third_party/imgui/examples/opengl2_example/imgui_impl_glfw.cpp
)

# enable exports so the plugin can link to the executable
set_target_properties(host_app PROPERTIES ENABLE_EXPORTS ON)

# add an include dir for ease of use
target_include_directories(host_app PUBLIC src)

# enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(host_app PRIVATE -Wall -Wextra)
else()
    target_compile_options(host_app PRIVATE /W4)
endif()

# defines needed for RCRL integration
target_compile_definitions(host_app PRIVATE "RCRL_PLUGIN_FILE=\"${plugin_file}\"")
target_compile_definitions(host_app PRIVATE "RCRL_PLUGIN_NAME=\"plugin\"")
target_compile_definitions(host_app PRIVATE "RCRL_BUILD_FOLDER=\"${PROJECT_BINARY_DIR}\"")
target_compile_definitions(host_app PRIVATE "RCRL_BIN_FOLDER=\"$<TARGET_FILE_DIR:host_app>/\"")
target_compile_definitions(host_app PRIVATE "RCRL_EXTENSION=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
if(${CMAKE_GENERATOR} MATCHES "Visual Studio" OR ${CMAKE_GENERATOR} MATCHES "Xcode")
    target_compile_definitions(host_app PRIVATE "RCRL_CONFIG=\"$<CONFIG>\"")
endif()
# unimportant - to construct a path to the fonts in the third party imgui folder
target_compile_definitions(host_app PRIVATE "CMAKE_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")

# so the host app exports symbols from its headers
target_compile_definitions(host_app PRIVATE "HOST_APP")

####################################################################################################
# the plugin that will be used for RCRL
####################################################################################################

# the plugin target that will be recompiled and loaded by RCRL
add_library(plugin SHARED EXCLUDE_FROM_ALL ${plugin_file} src/precompiled_for_plugin.cpp)
# link the RCRL plugin with the host app so we can call stuff from it
target_link_libraries(plugin host_app)
# exclude it even for Visual Studio when building the whole solution (EXCLUDE_FROM_ALL is not enough)
set_target_properties(plugin PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
# no 'lib' prefix for some compilers/platforms - simplifies my code
set_target_properties(plugin PROPERTIES PREFIX "")
if(APPLE)
    # weird flags to make it link to the executable - see this: https://stackoverflow.com/questions/48176641
    set_target_properties(plugin PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
elseif(UNIX)
    # add -fPIC - on some architectures under linux the precompiled header for the plugin target couldn't be used
    target_compile_options(plugin PRIVATE -fPIC)
elseif(MSVC)
    # we don't want .pdb files for the plugin because when we are within Visual Studio and debugging the application it locks
    # the .pdb for the original .dll and subsequent compiles fail even though we have loaded copies of the original .dll
    # can also use /PDBALTPATH - https://blog.molecular-matters.com/2014/05/10/using-runtime-compiled-c-code-as-a-scripting-language-under-the-hood/
    set_target_properties(plugin PROPERTIES LINK_FLAGS /DEBUG:NONE)
endif()
# add a precompiled header
add_precompiled_header(plugin ${CMAKE_CURRENT_SOURCE_DIR}/src/precompiled_for_plugin.h ${CMAKE_CURRENT_SOURCE_DIR}/src/precompiled_for_plugin.cpp)

####################################################################################################
# third party libs
####################################################################################################

# imgui
add_library(imgui STATIC src/third_party/imgui/imgui.cpp src/third_party/imgui/imgui_draw.cpp)
target_include_directories(imgui INTERFACE src/third_party/imgui)

# ImGuiColorTextEdit
add_library(ImGuiColorTextEdit STATIC src/third_party/ImGuiColorTextEdit/TextEditor.cpp src/third_party/ImGuiColorTextEdit/TextEditor.h)
target_link_libraries(ImGuiColorTextEdit PRIVATE imgui)

# glfw
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(src/third_party/glfw)

# tiny-process-library
add_subdirectory(src/third_party/tiny-process-library)

# folders for the third party libs
set_target_properties(glfw PROPERTIES FOLDER "third_party")
set_target_properties(imgui PROPERTIES FOLDER "third_party")
set_target_properties(ImGuiColorTextEdit PROPERTIES FOLDER "third_party")
set_target_properties(tiny-process-library PROPERTIES FOLDER "third_party")

# link host app to third party libs
find_package(OpenGL REQUIRED)
target_link_libraries(host_app PRIVATE imgui glfw ImGuiColorTextEdit tiny-process-library ${OPENGL_LIBRARIES})

####################################################################################################
# tests
####################################################################################################

option(RCRL_WITH_TESTS "Build tests for RCRL" ON)
if(RCRL_WITH_TESTS)
    enable_testing()
	add_subdirectory(tests)
endif()
