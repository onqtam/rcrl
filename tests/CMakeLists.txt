# this file should be used from the top CMakeLists.txt of the repository and assumes:
# - relative paths are correct
# - tiny-process-library is present
# - the plugin_file variable is defined

# parser tests
add_executable(rcrl_parser_tests ../src/rcrl/rcrl_parser.cpp parser_tests.cpp)
add_test(NAME rcrl_parser_tests COMMAND rcrl_parser_tests)

# compiler tests
add_executable(rcrl_compiler_tests ../src/rcrl/rcrl.cpp ../src/rcrl/rcrl_parser.cpp compiler_tests.cpp)
# needed defines
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_PLUGIN_FILE=\"${plugin_file}\"")
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_PLUGIN_PROJECT=\"test_plugin\"")
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_BUILD_FOLDER=\"${PROJECT_BINARY_DIR}\"")
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_BIN_FOLDER=\"$<TARGET_FILE_DIR:rcrl_compiler_tests>/\"")
target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_EXTENSION=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
if(${CMAKE_GENERATOR} MATCHES "Visual Studio" OR ${CMAKE_GENERATOR} MATCHES "Xcode")
	target_compile_definitions(rcrl_compiler_tests PRIVATE "RCRL_CONFIG=\"$<CONFIG>\"")
endif()
# link to process library
target_link_libraries(rcrl_compiler_tests PRIVATE tiny-process-library)
# export symbols from executable
set_target_properties(rcrl_compiler_tests PROPERTIES ENABLE_EXPORTS ON)
# add include directory
target_include_directories(rcrl_compiler_tests PUBLIC ../src)

# the test plugin
add_library(test_plugin SHARED EXCLUDE_FROM_ALL ${plugin_file})
target_link_libraries(test_plugin rcrl_compiler_tests)
set_target_properties(test_plugin PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(test_plugin PROPERTIES PREFIX "")
if(MSVC)
	set_target_properties(test_plugin PROPERTIES LINK_FLAGS /DEBUG:NONE)
endif()

add_test(NAME rcrl_compiler_tests COMMAND rcrl_compiler_tests)
