cmake_minimum_required(VERSION 3.0.0)

project(RCRL)

####################################################################################################
# global flags and directories setup
####################################################################################################

set(OUTPUT_DIR ${PROJECT_BINARY_DIR}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# latest c++ standards
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -fvisibility=hidden")
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
    src/main.cpp
    src/host_app.cpp
    src/host_app.h
    src/rcrl.cpp
    src/rcrl.h
    src/visibility.h
    src/third_party/imgui/examples/opengl2_example/imgui_impl_glfw.cpp
)
# enable exports so the plugin can link to the executable
set_target_properties(host_app PROPERTIES ENABLE_EXPORTS ON)
target_include_directories(host_app PUBLIC src)
# defines needed for easier RCRL integration
target_compile_definitions(host_app PRIVATE "RCRL_PLUGIN_FILE=\"${plugin_file}\"")
target_compile_definitions(host_app PRIVATE "RCRL_BUILD_FOLDER=\"${PROJECT_BINARY_DIR}\"")
target_compile_definitions(host_app PRIVATE "RCRL_BIN_FOLDER=\"$<TARGET_FILE_DIR:host_app>/\"")
target_compile_definitions(host_app PRIVATE "RCRL_HOST_APP")
target_compile_definitions(host_app PRIVATE "RCRL_EXTENSION=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
if(${CMAKE_GENERATOR} MATCHES "Visual Studio" OR ${CMAKE_GENERATOR} MATCHES "Xcode")
    target_compile_definitions(host_app PRIVATE "RCRL_CONFIG=\"$<CONFIG>\"")
endif()

####################################################################################################
# the plugin that will be used for RCRL
####################################################################################################

add_library(plugin SHARED EXCLUDE_FROM_ALL ${plugin_file})
# link the RCRL plugin with the host app so we can call stuff from it
target_link_libraries(plugin host_app)
# exclude it even for Visual Studio when building the whole solution (EXCLUDE_FROM_ALL is not enough)
set_target_properties(plugin PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
# no 'lib' prefix for some compilers/platforms - simplifies my code
set_target_properties(plugin PROPERTIES PREFIX "")
if(MSVC)
    # we don't want .pdb files for the plugin because when we are within Visual Studio and debugging the application it locks
    # the .pdb for the original .dll and subsequent compiles fail even though we have loaded copies of the original .dll
    # can also use /PDBALTPATH - https://blog.molecular-matters.com/2014/05/10/using-runtime-compiled-c-code-as-a-scripting-language-under-the-hood/
    set_target_properties(plugin PROPERTIES LINK_FLAGS /DEBUG:NONE)
endif()

####################################################################################################
# third party libs
####################################################################################################

# imgui
add_library(imgui STATIC src/third_party/imgui/imgui.cpp src/third_party/imgui/imgui_draw.cpp src/third_party/imgui/imgui_demo.cpp)
target_include_directories(imgui INTERFACE src/third_party/imgui)

# ImGuiColorTextEdit
add_library(ImGuiColorTextEdit STATIC src/third_party/ImGuiColorTextEdit/TextEditor.cpp src/third_party/ImGuiColorTextEdit/TextEditor.h)
target_link_libraries(ImGuiColorTextEdit PRIVATE imgui)

# hack until this gets merged: https://github.com/BalazsJako/ImGuiColorTextEdit/pull/7
if(MSVC)
    target_compile_options(ImGuiColorTextEdit PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/hack_for_imgui_color_text_edit.h")
else()
    target_compile_options(ImGuiColorTextEdit PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/hack_for_imgui_color_text_edit.h)
endif()

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
target_link_libraries(host_app PRIVATE imgui glfw ImGuiColorTextEdit tiny-process-library)
if(APPLE)
    target_link_libraries(host_app PRIVATE "-framework OpenGL")
elseif(UNIX)
    target_link_libraries(host_app PRIVATE dl GL)
else()
    target_link_libraries(host_app PRIVATE opengl32)
endif()
