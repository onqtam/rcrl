cmake_minimum_required(VERSION 3.0.0)

project(rcrl)

set(OUTPUT_DIR ${PROJECT_BINARY_DIR}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -fvisibility=hidden")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
endif()

set(plugin_file ${PROJECT_BINARY_DIR}/plugin.cpp)

add_executable(host_app src/main.cpp src/visibility.h src/host_app.h)
set_target_properties(host_app PROPERTIES ENABLE_EXPORTS ON) # so the plugin can link to the executable
target_compile_definitions(host_app PRIVATE "CRCL_PLUGIN_FILE=\"${plugin_file}\"")
target_compile_definitions(host_app PRIVATE "CRCL_BUILD_FOLDER=\"${PROJECT_BINARY_DIR}\"")
target_compile_definitions(host_app PRIVATE "CRCL_HOST_APP")
if(MSVC AND NOT ${CMAKE_GENERATOR} STREQUAL "Ninja" AND NOT CMAKE_GENERATOR STREQUAL "NMake Makefiles")
    target_compile_definitions(host_app PRIVATE "CRCL_CONFIG=\"--config $(Configuration)\"")
endif()
target_include_directories(host_app PUBLIC src)

file(WRITE ${plugin_file} "") # touch the file so it exists

add_library(plugin SHARED EXCLUDE_FROM_ALL ${plugin_file})
# link with the host app so we can call stuff from it
target_link_libraries(plugin host_app)
# exclude it even for Visual Studio when building the whole solution (EXCLUDE_FROM_ALL is not enough)
set_target_properties(plugin PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
if(MSVC)
    # we don't want .pdb files for the plugin because when we are within Visual Studio and debugging the application it locks
    # the .pdb for the original .dll and subsequent compiles fail even though we have loaded copies of the original .dll
    set_target_properties(plugin PROPERTIES LINK_FLAGS /DEBUG:NONE)
endif()
