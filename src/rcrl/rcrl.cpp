#include "rcrl.h"
#include "rcrl_parser.h"

#include <cassert>
#include <fstream>
#include <map>
#include <cctype>  // isspace()
#include <cstring> // strlen()
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <iostream> // TODO: to remove

#include <process.hpp>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HMODULE RCRL_Dynlib;
#define RDRL_LoadDynlib(lib) LoadLibrary(lib)
#define RCRL_CloseDynlib FreeLibrary
#define RCRL_CopyDynlib(src, dst) CopyFile(src, dst, false)
#define RCRL_System_Delete "del /Q "

#else

#include <dlfcn.h>
typedef void* RCRL_Dynlib;
#define RDRL_LoadDynlib(lib) dlopen(lib, RTLD_NOW)
#define RCRL_CloseDynlib dlclose
#define RCRL_CopyDynlib(src, dst) (!system((string("cp ") + src + " " + dst).c_str()))
#define RCRL_System_Delete "rm "

#endif

#ifdef _WIN32
#define RCRL_SYMBOL_EXPORT __declspec(dllexport)
#else
#define RCRL_SYMBOL_EXPORT __attribute__((visibility("default")))
#endif

using namespace std;

map<string, void*>                   rcrl_persistence;
vector<pair<void*, void (*)(void*)>> rcrl_deleters;

// for use by the rcrl plugin
RCRL_SYMBOL_EXPORT void*& rcrl_get_persistence(const char* var_name) { return rcrl_persistence[var_name]; }
RCRL_SYMBOL_EXPORT void   rcrl_add_deleter(void* address, void (*deleter)(void*)) {
    rcrl_deleters.push_back({address, deleter});
}

namespace rcrl
{
vector<pair<string, RCRL_Dynlib>>   g_plugins;
unique_ptr<TinyProcessLib::Process> compiler_process;
string                              compiler_output;
mutex                               compiler_output_mut;

void output_appender(const char* bytes, size_t n) {
    lock_guard<mutex> lock(compiler_output_mut);
    compiler_output += string(bytes, n);
}

bool                       last_compile_successful = false;
vector<string>             compiled_sections;
vector<pair<string, Mode>> uncompiled_sections;

void cleanup_plugins() {
    assert(!is_compiling());

    // call the deleters in reverse order
    for(auto it = rcrl_deleters.rbegin(); it != rcrl_deleters.rend(); ++it)
        it->second(it->first);
    rcrl_deleters.clear();

    // clear the code sections and pointers to globals
    compiled_sections.clear();
    rcrl_persistence.clear();

    // close the plugins in reverse order
    for(auto it = g_plugins.rbegin(); it != g_plugins.rend(); ++it)
        RCRL_CloseDynlib(it->second);

    string bin_folder(RCRL_BIN_FOLDER);
#ifdef _WIN32
    // replace forward slash with windows style slash
    replace(bin_folder.begin(), bin_folder.end(), '/', '\\');
#endif // _WIN32

    if(g_plugins.size())
        system((string(RCRL_System_Delete) + bin_folder + RCRL_PLUGIN_PROJECT "_*" RCRL_EXTENSION).c_str());
    g_plugins.clear();
}

bool submit_code(string code, Mode mode) {
    assert(!is_compiling());
    assert(code.size());

    // if this is the first piece of code submitted
    if(compiled_sections.size() == 0)
        compiled_sections.push_back("#include \"rcrl/rcrl_for_plugin.h\"\n");

    // fix line endings
    replace(code.begin(), code.end(), '\r', '\n');

    // figure out the sections
    auto section_beginings = remove_comments(code);
    if(mode != FROM_COMMENTS) {
        section_beginings.clear();
        section_beginings.push_back({0, mode});
    }

    // fill the current sections of code for compilation
    uncompiled_sections.clear();
    for(auto it = section_beginings.begin(); it != section_beginings.end(); ++it) {
        // get the code
        string section_code = code.substr(
                it->first, (it + 1 == section_beginings.end() ? code.size() - it->first : (it + 1)->first - it->first));

        // for nicer output
        if(section_code.back() != '\n')
            section_code.push_back('\n');

        if(it->second == ONCE)
            section_code = "RCRL_ONCE_BEGIN\n" + section_code + "RCRL_ONCE_END\n";

        if(it->second == VARS) {
            try {
                auto vars = parse_vars(section_code);
                section_code.clear();

                for(const auto& var : vars) {
                    if(var.type == "auto" || var.type == "const auto")
                        section_code += "RCRL_VAR_AUTO(" + var.name + ", " +
                                        (var.type == "auto" ? "RCRL_EMPTY()" : "const") + ", " +
                                        (var.has_assignment ? "=" : "RCRL_EMPTY()") + ", " + var.initializer + ");\n";
                    else
                        section_code += "RCRL_VAR((" + var.type + "), " + var.name + ", " +
                                        (var.initializer.size() ? var.initializer : "RCRL_EMPTY()") + ");\n";
                }
            } catch(exception& e) {
                output_appender(e.what(), strlen(e.what()));
                uncompiled_sections.clear();
                return true;
            }
        }

        // push the section code to the list of uncompiled ones
        uncompiled_sections.push_back({section_code, it->second});
    }

    // concatenate all the sections to make the source file to be compiled
    ofstream myfile(RCRL_PLUGIN_FILE);
    for(const auto& section : compiled_sections)
        myfile << section;
    for(const auto& section : uncompiled_sections)
        myfile << section.first;
    myfile.close();

    // mark the successful compilation flag as false
    last_compile_successful = false;

    compiler_output.clear();
    compiler_process = unique_ptr<TinyProcessLib::Process>(
            new TinyProcessLib::Process("cmake --build " RCRL_BUILD_FOLDER " --target " RCRL_PLUGIN_PROJECT
#ifdef RCRL_CONFIG
                                        " --config " RCRL_CONFIG
#endif // multi config IDE
#if defined(RCRL_CONFIG) && defined(_MSC_VER)
                                        " -- /verbosity:quiet /consoleloggerparameters:PerformanceSummary"
#endif // Visual Studio
                                        ,
                                        "", output_appender, output_appender));

    return false;
}

string get_new_compiler_output() {
    lock_guard<mutex> lock(compiler_output_mut);
    auto              temp = compiler_output;
    compiler_output.clear();
    return temp;
}

bool is_compiling() { return compiler_process != nullptr; }

bool try_get_exit_status_from_compile(int& exitcode) {
    if(compiler_process && compiler_process->try_get_exit_status(exitcode)) {
        // remove the compiler process
        compiler_process.reset();

        last_compile_successful = exitcode == 0;

        return true;
    }
    return false;
}

void copy_and_load_new_plugin() {
    assert(!is_compiling());
    assert(last_compile_successful);

    last_compile_successful = false; // shouldn't call this function twice in a row without compiling anything in between

    for(const auto& section : uncompiled_sections)
        if(section.second != ONCE)
            compiled_sections.push_back(section.first);

    // copy the plugin
    auto       name_copied = string(RCRL_BIN_FOLDER) + RCRL_PLUGIN_PROJECT "_" + to_string(g_plugins.size()) + RCRL_EXTENSION;
    const auto copy_res = RCRL_CopyDynlib((string(RCRL_BIN_FOLDER) + RCRL_PLUGIN_PROJECT RCRL_EXTENSION).c_str(), name_copied.c_str());
    assert(copy_res);

    // load the plugin
    auto plugin = RDRL_LoadDynlib(name_copied.c_str());
    assert(plugin);

    // add the plugin to the list of loaded ones - for later unloading
    g_plugins.push_back({name_copied, plugin});
}
} // namespace rcrl
