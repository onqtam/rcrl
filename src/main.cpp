#include "host_app.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cassert>
using namespace std;

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>

typedef HMODULE DynamicLib;

#define LoadDynlib(lib) LoadLibrary(lib)
#define CloseDynlib FreeLibrary
#define GetProc GetProcAddress
#define CopyDynlib(src, dst) CopyFile(src, dst, false)

#define PLUGIN_EXTENSION ".dll"
#define FILESYSTEM_SLASH "\\"

#else // _WIN32

#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>

typedef void* DynamicLib;

#define LoadDynlib(lib) dlopen(lib, RTLD_NOW)
#define CloseDynlib dlclose
#define GetProc dlsym
#define CopyDynlib(src, dst)                                                                       \
    (!system((string("cp ") + get_path_to_exe() + src + " " + get_path_to_exe() + dst).c_str()))

#define PLUGIN_EXTENSION ".so"
#define FILESYSTEM_SLASH "/"

#endif // _WIN32

string get_path_to_exe() {
#ifdef _WIN32
    TCHAR path[MAX_PATH];
    GetModuleFileName(nullptr, path, MAX_PATH);
#else  // _WIN32
    char arg1[20];
    char path[PATH_MAX + 1] = {0};
    sprintf(arg1, "/proc/%d/exe", getpid());
    auto dummy = readlink(arg1, path, 1024);
    ((void)dummy); // to use it... because the result is annotated for static analysis
#endif // _WIN32
    string spath = path;
    size_t pos   = spath.find_last_of("\\/");
    assert(pos != std::string::npos);
    return spath.substr(0, pos) + FILESYSTEM_SLASH;
}

void f() { cout << "forced print!" << endl; }

void reconstruct_plugin_source_file() {
    ofstream myfile(CRCL_PLUGIN_FILE);
    myfile << "#include \"host_app.h\"\n";
    myfile.close();
}

int main() {
    // init the plugin file
    reconstruct_plugin_source_file();

    vector<pair<string, DynamicLib>> plugins;
    string                           line;

    getline(cin, line);
    while(line != "q") {
        ofstream myfile(CRCL_PLUGIN_FILE, ios::out | ios::app);
        myfile << line << endl;
        myfile.close();

        system("cls");

        system("cmake --build " CRCL_BUILD_FOLDER " --target plugin " CRCL_ADDITIONAL_FLAGS);

        //std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        auto plugin_name =
                get_path_to_exe() + "plugin_" + to_string(plugins.size()) + PLUGIN_EXTENSION;
        const auto copy_res = CopyDynlib((get_path_to_exe() + "plugin" PLUGIN_EXTENSION).c_str(),
                                         plugin_name.c_str());

        assert(copy_res);

        auto plugin = LoadDynlib(plugin_name.c_str());
        assert(plugin);

        plugins.push_back({plugin_name, plugin});

        getline(cin, line);
    }

    // cleanup plugins
    for(auto plugin : plugins)
        CloseDynlib(plugin.second);

    if(plugins.size())
        system((string("del ") + get_path_to_exe() + "plugin_*" PLUGIN_EXTENSION " /Q").c_str());
    plugins.clear();

    return 0;
}
