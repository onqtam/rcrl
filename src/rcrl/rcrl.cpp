#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "rcrl.h"
#include "rcrl_parser.h"

#include <cassert>
#include <fstream>
#include <map>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <sstream>

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

static map<string, void*>                   persistence;
static vector<pair<void*, void (*)(void*)>> deleters;

// for use by the rcrl plugin
RCRL_SYMBOL_EXPORT void*& rcrl_get_persistence(const char* var_name) { return persistence[var_name]; }
RCRL_SYMBOL_EXPORT void   rcrl_add_deleter(void* address, void (*deleter)(void*)) { deleters.push_back({address, deleter}); }

namespace rcrl
{
// global state
static vector<pair<string, RCRL_Dynlib>>   plugins;
static unique_ptr<TinyProcessLib::Process> compiler_process;
static string                              compiler_output;
static mutex                               compiler_output_mut;
static bool                                last_compile_successful = false;
// holds code only for global and vars sections which have already been successfully compiled and loaded
static vector<string> compiled_sections;
// holds all the sections which were last submitted for compilation - on success and if the
// new plugin is loaded global and vars sections will be put in the compiled_sections list
static vector<pair<string, Mode>> uncompiled_sections;

// called asynchronously by the compilation process
void output_appender(const char* bytes, size_t n) {
    lock_guard<mutex> lock(compiler_output_mut);
    compiler_output += string(bytes, n);
}

std::string cleanup_plugins(bool redirect_stdout) {
    assert(!is_compiling());

    if(redirect_stdout)
        freopen(RCRL_BUILD_FOLDER "/rcrl_stdout.txt", "w", stdout);

    // call the deleters in reverse order
    for(auto it = deleters.rbegin(); it != deleters.rend(); ++it)
        it->second(it->first);
    deleters.clear();

    // clear the code sections and pointers to globals
    compiled_sections.clear();
    persistence.clear();

    // close the plugins in reverse order
    for(auto it = plugins.rbegin(); it != plugins.rend(); ++it)
        RCRL_CloseDynlib(it->second);

    string out;

    if(redirect_stdout) {
        fclose(stdout);
        freopen("CON", "w", stdout);

        FILE* f = fopen(RCRL_BUILD_FOLDER "/rcrl_stdout.txt", "rb");
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        out.resize(fsize);
        fread((void*)out.data(), fsize, 1, f);
        fclose(f);
    }

    string bin_folder(RCRL_BIN_FOLDER);
#ifdef _WIN32
    // replace forward slash with windows style slash
    replace(bin_folder.begin(), bin_folder.end(), '/', '\\');
#endif // _WIN32

    if(plugins.size())
        system((string(RCRL_System_Delete) + bin_folder + RCRL_PLUGIN_NAME "_*" RCRL_EXTENSION).c_str());
    plugins.clear();

    return out;
}

bool submit_code(string code, Mode default_mode, bool* used_default_mode) {
    assert(!is_compiling());
    assert(code.size());

    // if this is the first piece of code submitted
    if(compiled_sections.size() == 0)
        compiled_sections.push_back("#include \"rcrl/rcrl_for_plugin.h\"\n");

    // fix line endings
    replace(code.begin(), code.end(), '\r', '\n');

    // figure out the sections
    auto section_beginings = parse_sections_and_remove_comments(code, default_mode);

    // fill the current sections of code for compilation
    uncompiled_sections.clear();
    for(auto it = section_beginings.begin(); it != section_beginings.end(); ++it) {
        // get the code
        string section_code =
                code.substr(it->start_idx, (it + 1 == section_beginings.end() ? code.size() - it->start_idx :
                                                                                (it + 1)->start_idx - it->start_idx));

        if(used_default_mode && it == section_beginings.begin())
            *used_default_mode = section_code.find_first_not_of(" \t\n\v\f\r") != string::npos;

        // for nicer output
        if(section_code.back() != '\n')
            section_code.push_back('\n');

        if(it->mode == ONCE)
            section_code = "RCRL_ONCE_BEGIN\n" + section_code + "RCRL_ONCE_END\n";

        if(it->mode == VARS) {
            try {
                auto vars = parse_vars(section_code, it->line);
                section_code.clear();

                for(const auto& var : vars) {
                    if(var.type == "auto" || var.type == "const auto") {
                        section_code += (var.is_reference ? "RCRL_VAR_AUTO_REF(" : "RCRL_VAR_AUTO(") + var.name + ", " +
                                        (var.type == "auto" ? "RCRL_EMPTY()" : "const") + ", " +
                                        (var.has_assignment ? "=" : "RCRL_EMPTY()") + ", " + var.initializer + ");\n";
                    } else {
                        section_code += "RCRL_VAR((" + var.type + (var.is_reference ? "*" : "") + "), (" + var.type + "), " +
                                        (var.is_reference ? "*" : "RCRL_EMPTY()") + ", " + var.name + ", " +
                                        (var.initializer.size() ? var.initializer : "RCRL_EMPTY()") + ");\n";
                    }
                }
            } catch(exception& e) {
                output_appender(e.what(), strlen(e.what()));
                uncompiled_sections.clear();
                return false;
            }
        }

        // push the section code to the list of uncompiled ones
        uncompiled_sections.push_back({section_code, it->mode});
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
            new TinyProcessLib::Process("cmake --build " RCRL_BUILD_FOLDER " --target " RCRL_PLUGIN_NAME
#ifdef RCRL_CONFIG
                                        " --config " RCRL_CONFIG
#endif // multi config IDE
#if defined(RCRL_CONFIG) && defined(_MSC_VER)
                                        " -- /verbosity:quiet"
#endif // Visual Studio
                                        ,
                                        "", output_appender, output_appender));

    return true;
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

string copy_and_load_new_plugin(bool redirect_stdout) {
    assert(!is_compiling());
    assert(last_compile_successful);

    last_compile_successful = false; // shouldn't call this function twice in a row without compiling anything in between

    for(const auto& section : uncompiled_sections)
        if(section.second != ONCE)
            compiled_sections.push_back(section.first);

    // copy the plugin
    auto       name_copied = string(RCRL_BIN_FOLDER) + RCRL_PLUGIN_NAME "_" + to_string(plugins.size()) + RCRL_EXTENSION;
    const auto copy_res =
            RCRL_CopyDynlib((string(RCRL_BIN_FOLDER) + RCRL_PLUGIN_NAME RCRL_EXTENSION).c_str(), name_copied.c_str());
    assert(copy_res);

    if(redirect_stdout)
        freopen(RCRL_BUILD_FOLDER "/rcrl_stdout.txt", "w", stdout);

    // load the plugin
    auto plugin = RDRL_LoadDynlib(name_copied.c_str());
    assert(plugin);

    // add the plugin to the list of loaded ones - for later unloading
    plugins.push_back({name_copied, plugin});

    string out;

    if(redirect_stdout) {
        fclose(stdout);
        freopen("CON", "w", stdout);

        FILE* f = fopen(RCRL_BUILD_FOLDER "/rcrl_stdout.txt", "rb");
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        out.resize(fsize);
        fread((void*)out.data(), fsize, 1, f);
        fclose(f);
    }

    return out;
}
} // namespace rcrl
