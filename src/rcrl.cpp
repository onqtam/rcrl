#include "rcrl.h"

#include "visibility.h"

#include <cassert>
#include <fstream>
#include <map>
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

// for use by the rcrl plugin
SYMBOL_EXPORT std::map<std::string, void*> rcrl_persistence;
SYMBOL_EXPORT std::vector<std::pair<void*, void (*)(void*)>> rcrl_deleters;

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

bool           last_compile_successful = false;
vector<string> sections;
string         current_section;
Mode           current_section_mode;

void cleanup_plugins() {
    assert(!is_compiling());

    // call the deleters in reverse order
    for(auto it = rcrl_deleters.rbegin(); it != rcrl_deleters.rend(); ++it)
        it->second(it->first);
    rcrl_deleters.clear();

    // clear the code sections and pointers to globals
    sections.clear();
    rcrl_persistence.clear();

    // close the plugins in reverse order
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

void submit_code(string code, Mode mode) {
    assert(!is_compiling());
    assert(code.size());

    // if this is the first piece of code submitted
    if(sections.size() == 0) {
        sections.push_back("#include \"rcrl_for_plugin.h\"\n");
    }

    std::replace(code.begin(), code.end(), '\r', '\n');
    if(code.back() != '\n')
        code.push_back('\n');

    current_section_mode = mode;
    current_section.clear();
    if(mode == GLOBAL) {
        current_section += code;
    } else if(mode == ONCE) {
        current_section += "\nRCRL_ONCE_BEGIN\n";
        current_section += code;
        current_section += "RCRL_ONCE_END\n";
    }
    if(mode == VARS) {
        auto type_ender = code.find_first_of(" ");
        auto name_ender = code.find_first_of(";{(=", type_ender);

        string type = code.substr(0, type_ender);
        string name = code.substr(type_ender + 1, name_ender - type_ender - 1);

        current_section += type + "* " + name + "_ptr = [](){\n";
        current_section += "    auto& address = rcrl_persistence[\"" + name + "\"];\n";
        current_section += "    if (address == nullptr) {\n";
        current_section += "        address = new " + type + "();\n";
        current_section += "        rcrl_deleters.push_back({address, rcrl_deleter<" + type + ">});\n";
        current_section += "    }\n";
        current_section += "    return static_cast<" + type + "*>(address);\n";
        current_section += "}();\n";
        current_section += type + "& " + name + " = *" + name + "_ptr;\n\n";
    }

    // concatenate all the sections to make the source file to be compiled
    ofstream myfile(RCRL_PLUGIN_FILE);
    for(auto& section : sections)
        myfile << section;
    myfile << current_section;
    myfile.close();

    // mark the successful compilation flag as false
    last_compile_successful = false;

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

        last_compile_successful = exitcode == 0;

        return true;
    }
    return false;
}

void copy_and_load_new_plugin() {
    assert(!is_compiling());
    assert(last_compile_successful);

    last_compile_successful = false; // shouldn't call this function twice in a row without compiling anything in between

    if(current_section_mode != ONCE) {
        sections.push_back(current_section);
    }

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

// NOT USED FOR ANYTHING YET - parses (maybe) properly comments/strings/chars
void parse_vars(const vector<char>& text) {
    bool in_char                = false; // 'c'
    bool in_string              = false; // "str"
    bool in_comment             = false;
    bool in_single_line_comment = false;
    bool in_multi_line_comment  = false;

    int current_line_start = 0;

    vector<pair<char, int>> braces; // the current active stack of braces

    vector<int> semicolons; // the positions of all semicolons

    for(size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];

        if(c == '"' && !in_comment && !in_char) {
            if(!in_string) {
                in_string = true;
                //goto state_changed;
            } else {
                int num_slashes = 0;
                while(text[i - 1 - num_slashes] == '\\') {
                    ++num_slashes;
                }

                if(num_slashes % 2 == 0) {
                    in_string = false;
                    //goto state_changed;
                }
            }
        }
        if(c == '\'' && !in_comment && !in_string) {
            if(!in_char) {
                in_char = true;
                //goto state_changed;
            } else {
                if(text[i - 1] != '\\') {
                    in_char = false;
                    //goto state_changed;
                }
            }
        }
        if(c == '/') {
            if(!in_comment && !in_char && !in_string && i > 0 && text[i - 1] == '/') {
                in_single_line_comment = true;
                //goto state_changed;
            }
            if(in_multi_line_comment && text[i - 1] == '*') {
                in_multi_line_comment = false;
                //goto state_changed;
            }
        }
        if(c == '*') {
            if(!in_comment && !in_char && !in_string && i > 0 && text[i - 1] == '/') {
                in_multi_line_comment = true;
                //goto state_changed;
            }
        }
        if(c == '\n') {
            if(in_single_line_comment && text[i - 1] != '\\') {
                in_single_line_comment = false;
                //goto state_changed;
            }
        }

        // proceed with tokens and code
        if(!in_string && !in_char && !in_comment) {
            if(c == '(' || c == '[' || c == '{') {
                braces.push_back({c, i});
            }
            if(c == ')' || c == ']' || c == '}') {
                assert(braces.back().first == c);
                braces.pop_back();
            }
            if(c == ';') {
                semicolons.push_back(i);
            }
        }

        //state_changed:
        // state for the next iteration of the loop
        in_comment = in_single_line_comment && in_multi_line_comment;
        current_line_start += (c == '\n') ? (i + 1 - current_line_start) : 0;

        // assert for consistency
        assert(!in_char || (in_char && !in_comment && !in_string));
        assert(!in_string || (in_string && !in_char && !in_comment));
        assert(!in_comment || (in_comment && !in_char && !in_string));
    }

    assert(braces.size() == 0);
}

} // namespace rcrl