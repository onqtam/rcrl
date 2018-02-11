#pragma once

#include <string>

// RCRL assumes that the following preprocessor identifiers are defined (easy with CMake):
// - RCRL_PLUGIN_FILE - the full path to the .cpp file used for compilation
// - RCRL_PLUGIN_NAME - the name of the CMake target of the plugin
// - RCRL_BUILD_FOLDER - the root build folder where CMake has generated the build files
// - RCRL_BIN_FOLDER - the folder with compiled binaries - the plugin will be copied/loaded from there
// - RCRL_EXTENSION - the shared object extension - '.dll' for Windows, '.so' for Linux and '.dylib' for macOS
// - RCRL_CONFIG - optional - if the current build system supports multiple configurations at once (Visual Studio, XCode)

namespace rcrl
{
enum Mode
{
    GLOBAL,
    VARS,
    ONCE
};

// Cleanup:
// - calls the destructors of persistent variables
// - unloads the plugins and deletes them from the filesystem
// - can optionally redirect stdout only while unloading the plugins (for destructors) (uses a temp .txt file) - and returns it
// Shouldn't be called if:
// - compilation is in progress
std::string cleanup_plugins(bool redirect_stdout = false);

// Submits code for compilation:
// - parses the code for the 3 different sections in single line comments: // global/vars/once
//   with the default mode for the begining so such an annotation can be skipped for the first section
// - parses variable definitions from 'vars' sections - that can lead to parser errors on invalid input
//   parsing errors can be obtained through rcrl::get_new_compiler_output()
// - submits the sections for compilation in a non-blocking way using 'tiny-process-library' for the process
// - returns true if the parsing succeeds and the compilation is started
// - can optionally tell if the default mode was actually used (not used when the first thing in
//   the code is an explicit section change in a comment) - through the optional boolean pointer
// Shouldn't be called if:
// - compilation is in progress
// - code is empty
bool submit_code(std::string code, Mode default_mode = ONCE, bool* used_default_mode = nullptr);

// Returns any new compiler output, since it's done in a background thread (also returns parser errors)
std::string get_new_compiler_output();

// Returns true if compilation is in progress
bool is_compiling();

// Used to obtain the result of the current running compilation:
// - non-blocking
// - returns true if the compilation has recently ended
// - returns false if compilation is still in progress or nothing is being compiled
// - the compile status is obtained through the output parameter
// Note that if it has returned true and called again immediately after that without a new compilation
// being started - it will return false - so make sure to use the result exit code from when it returns true
bool try_get_exit_status_from_compile(int& exitcode);

// Copies the plugin from the last successful compilation with a new name and loads it:
// - can optionally redirect stdout only while loading the plugin (uses a temp .txt file) - and returns it
// Shouldn't be called if:
// - compilation is in progress
// - the last compilation was unsuccessful (use the exit code from rcrl::try_get_exit_status_from_compile() to determine that)
// - the plugin from the last compilation has already been loaded
std::string copy_and_load_new_plugin(bool redirect_stdout = false);
} // namespace rcrl