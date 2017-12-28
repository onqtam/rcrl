#include "rcrl.h"

#include <cassert>
#include <fstream>
#include <algorithm>

#include <third_party/tiny-process-library/process.hpp>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef HMODULE RCRL_Dynlib;

#define RDRL_LoadDynlib(lib) LoadLibrary(lib)
#define RCRL_CloseDynlib FreeLibrary
#define RCRL_CopyDynlib(src, dst) CopyFile(src, dst, false)

#else // _WIN32

#include <dlfcn.h>

typedef void* RCRL_Dynlib;

#define RDRL_LoadDynlib(lib) dlopen(lib, RTLD_NOW)
#define RCRL_CloseDynlib dlclose
#define RCRL_CopyDynlib(src, dst) (!system((string("cp ") + RCRL_BIN_FOLDER + src + " " + RCRL_BIN_FOLDER + dst).c_str()))

#endif // _WIN32

using namespace std;

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

void cleanup_plugins() {
    for(auto it = g_plugins.rbegin(); it != g_plugins.rend(); ++it)
        RCRL_CloseDynlib(it->second);

    string bin_folder(RCRL_BIN_FOLDER);
#ifdef _WIN32
	// replace forward slash with windows style slash
    std::replace(bin_folder.begin(), bin_folder.end(), '/', '\\');
#endif // _WIN32

    if(g_plugins.size())
        system((string("del ") + bin_folder + "plugin_*" RCRL_EXTENSION " /Q").c_str());
    g_plugins.clear();
}

void reconstruct_plugin_source_file() {
    ofstream myfile(RCRL_PLUGIN_FILE);
    myfile << "#include \"host_app.h\"\n";
    myfile.close();
}

void submit_code(const string& code, Mode mode) {
    ofstream myfile(RCRL_PLUGIN_FILE, ios::out | ios::app);
    myfile << code << endl;
    myfile.close();

    compiler_output.clear();
    compiler_process =
            make_unique<TinyProcessLib::Process>("cmake --build " RCRL_BUILD_FOLDER " --target plugin"
#ifdef RCRL_CONFIG
                                                 " --config " RCRL_CONFIG
#endif // multi config IDE
#if defined(RCRL_CONFIG) && defined(_MSC_VER)
                                                 " -- /verbosity:quiet /consoleloggerparameters:PerformanceSummary"
#endif // Visual Studio
                                                 ,
                                                 "", output_appender, output_appender);
}

std::string get_new_compiler_output() {
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
        return true;
    }
    return false;
}

void copy_and_load_new_plugin() {
    // copy the plugin
    auto       name_copied = string(RCRL_BIN_FOLDER) + "plugin_" + to_string(g_plugins.size()) + RCRL_EXTENSION;
    const auto copy_res = RCRL_CopyDynlib((string(RCRL_BIN_FOLDER) + "plugin" RCRL_EXTENSION).c_str(), name_copied.c_str());
    assert(copy_res);

    // load the plugin
    auto plugin = RDRL_LoadDynlib(name_copied.c_str());
    assert(plugin);

    // add the plugin to the list of loaded ones - for later unloading
    g_plugins.push_back({name_copied, plugin});
}
} // namespace rcrl