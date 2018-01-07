#include "rcrl.h"

#include "visibility.h"

#include <cassert>
#include <fstream>
#include <map>
#include <cctype>  // isspace()
#include <cstring> // strlen()
#include <stdexcept>
#include <algorithm>
#include <iostream> // TODO: to remove

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

map<string, void*>                   rcrl_persistence;
vector<pair<void*, void (*)(void*)>> rcrl_deleters;

// for use by the rcrl plugin
SYMBOL_EXPORT void*& rcrl_get_persistence(const char* var) { return rcrl_persistence[var]; }
SYMBOL_EXPORT void   rcrl_add_deleter(void* address, void (*deleter)(void*)) { rcrl_deleters.push_back({address, deleter}); }

namespace rcrl
{
struct VariableDefinition
{
    string type;
    string name;
    string initializer;
};

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
    replace(bin_folder.begin(), bin_folder.end(), '/', '\\');
#endif // _WIN32

    if(g_plugins.size())
        system((string("del ") + bin_folder + "plugin_*" RCRL_EXTENSION " /Q").c_str());
    g_plugins.clear();
}

vector<VariableDefinition> parse_vars(string text);

bool submit_code(string code, Mode mode) {
    assert(!is_compiling());
    assert(code.size());

    // if this is the first piece of code submitted
    if(sections.size() == 0) {
        sections.push_back("#include \"rcrl_for_plugin.h\"\n");
    }

    replace(code.begin(), code.end(), '\r', '\n');
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
        try {
            auto vars = parse_vars(code);

            for(const auto& var : vars) {
                current_section += "RCRL_VAR((" + var.type + "), " + var.name + ", " +
                                   (var.initializer.size() ? var.initializer : "RCRL_EMPTY()") + ");\n";
            }
        } catch(exception& e) {
            output_appender(e.what(), strlen(e.what()));
            return true;
        }
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

// ======================================================================================
// == WHAT FOLLOWS IS HORRIBLE CODE FOR PARSING VARIABLES AND THEIR TYPES/INITIALIZERS ==
// ======================================================================================

void ltrim(string& s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](int ch) { return !isspace(ch); }));
}

void rtrim(string& s) {
    s.erase(find_if(s.rbegin(), s.rend(), [](int ch) { return !isspace(ch); }).base(), s.end());
}

void trim(string& s) {
    ltrim(s);
    rtrim(s);
}

string remove_comments(const string& text) {
    string out(text);

    bool in_char                = false; // 'c'
    bool in_string              = false; // "str"
    bool in_comment             = false;
    bool in_single_line_comment = false;
    bool in_multi_line_comment  = false;

    for(size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];

        in_comment = in_single_line_comment || in_multi_line_comment;

        // assert for consistency
        assert(!in_char || (in_char && !in_comment && !in_string));
        assert(!in_string || (in_string && !in_char && !in_comment));
        assert(!in_comment || (in_comment && !in_char && !in_string));

        if(c == '"' && !in_comment && !in_char) {
            if(!in_string) {
                in_string = true;
            } else {
                int num_slashes = 0;
                while(text[i - 1 - num_slashes] == '\\') {
                    ++num_slashes;
                }

                if(num_slashes % 2 == 0) {
                    in_string = false;
                }
            }
        }
        if(c == '\'' && !in_comment && !in_string) {
            if(!in_char) {
                in_char = true;
            } else {
                if(text[i - 1] != '\\') {
                    in_char = false;
                }
            }
        }
        if(c == '/') {
            if(!in_comment && !in_char && !in_string && i > 0 && text[i - 1] == '/') {
                in_single_line_comment = true;
                out[i - 1]             = ' '; // substitute last character with whitespace - apparently we are in a comment
                out[i]                 = ' ';
                continue;
            }
            if(in_multi_line_comment && text[i - 1] == '*') {
                in_multi_line_comment = false;
                out[i]                = ' '; // append whitespace
                continue;
            }
        }
        if(c == '*') {
            if(!in_comment && !in_char && !in_string && i > 0 && text[i - 1] == '/') {
                in_multi_line_comment = true;
                out[i - 1]            = ' '; // substitute last character with whitespace - apparently we are in a comment
                out[i]                = ' ';
                continue;
            }
        }
        if(c == '\n') {
            if(in_single_line_comment && text[i - 1] != '\\') {
                in_single_line_comment = false;
                continue;
            }
        }

        if(in_comment) {
            // contents of comments are turned into whitespace
            out[i] = ' ';
        }
    }

    return out;
}

vector<VariableDefinition> parse_vars(string text) {
    vector<VariableDefinition> out;

    bool in_char       = false; // 'c'
    bool in_string     = false; // "str"
    bool in_whitespace = false;

    bool just_enterned_char_string = false;

    vector<pair<char, int>> braces; // the current active stack of braces

    vector<int> semicolons;        // the positions of all semicolons
    vector<int> whitespace_begins; // the positions of all whitespace beginings
    vector<int> whitespace_ends;   // the positions of all whitespace endings

    text = remove_comments(text);

    VariableDefinition current_var;
    bool               in_var               = false;
    size_t             current_var_name_end = 0;

    for(size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];

        if(c == '"' && !in_char) {
            if(!in_string) {
                in_string                 = true;
                just_enterned_char_string = true;
            } else {
                int num_slashes = 0;
                while(text[i - 1 - num_slashes] == '\\') {
                    ++num_slashes;
                }

                if(num_slashes % 2 == 0) {
                    in_string = false;
                }
            }
        }
        if(c == '\'' && !in_string) {
            if(!in_char) {
                in_char                   = true;
                just_enterned_char_string = true;
            } else {
                if(text[i - 1] != '\\') {
                    in_char = false;
                }
            }
        }

        // proceed with parsing variable definitions
        if(!in_string && !in_char) {
            if(braces.size() == 0 && (c == ';' || c == '(' || c == '{' || c == '=')) {
                // if after the name of a variable
                if(!in_var) {
                    if(whitespace_ends.size() == 0)
                        throw runtime_error("parse error - expected <type> <name>... with atleast 1 space in between");

                    auto var_name_begin  = whitespace_ends.back();
                    auto var_name_len    = i - var_name_begin;
                    current_var_name_end = i;
                    in_var               = true;

                    current_var.name = text.substr(var_name_begin, var_name_len);
                    trim(current_var.name);
                    int type_begin   = semicolons.size() ? semicolons.back() + 1 : 0;
                    current_var.type = text.substr(type_begin, var_name_begin - type_begin);
                    trim(current_var.type);
                }

                // if we are finalizing the variable - check if there is anything for its initialization
                if(c == ';' && in_var) {
                    current_var.initializer = text.substr(current_var_name_end, i - current_var_name_end);
                    trim(current_var.initializer);
                    if(current_var.initializer.size() && current_var.initializer.front() == '=') {
                        current_var.initializer.erase(current_var.initializer.begin());
                        trim(current_var.initializer);
                    }

                    if(current_var.initializer.size() && current_var.initializer.front() != '(' &&
                       current_var.initializer.front() != '{') {
                        current_var.initializer = "(" + current_var.initializer + ")";
                    }

                    // TODO: remove debug prints
                    cout << current_var.type << endl;
                    cout << current_var.name << endl;
                    cout << current_var.initializer << endl;

                    // var parsed
                    out.push_back(current_var);

                    in_var = false;
                }
            }

            if(c == '(' || c == '[' || c == '{') {
                braces.push_back({c, i});
            }
            if(c == ')' || c == ']' || c == '}') {
                // check that we are closing the right bracket
                if(braces.size() == 0)
                    throw runtime_error("parse error - encountered closing brace without an opening one");
                if(braces.back().first != (c == ')' ? '(' : (c == '}' ? '{' : '[')))
                    throw runtime_error("parse error - closing brace mismatch");
                braces.pop_back();
            }
            if(c == ';') {
                semicolons.push_back(i);
            }
        }

        if(!in_string && !in_char || just_enterned_char_string) {
            // if the current char is not a whitespace, but the previous one was - mark the end of a whitespace block
            if(!isspace(c) && (i == 0 || isspace(text[i - 1]))) {
                in_whitespace = false;
                whitespace_ends.push_back(i);
                if(i == 0)
                    whitespace_begins.push_back(i); // handle corner case
                assert(whitespace_begins.size() == whitespace_ends.size());
            }
            // if the current char is a whitespace, but the previous wasn't - mark the start of a whitespace block
            if(isspace(c) && (i == 0 || !isspace(text[i - 1]))) {
                whitespace_begins.push_back(i);
                in_whitespace = true;
            }
        }

        just_enterned_char_string = false;

        // assert for consistency
        assert(!in_char || (in_char && !in_string));
        assert(!in_string || (in_string && !in_char));
    }

    if(in_whitespace)
        whitespace_ends.push_back(text.size() - 1);

    assert(whitespace_begins.size() == whitespace_ends.size());
    if(in_var)
        throw runtime_error("parse error - parsing of variables not finished");
    if(braces.size() != 0)
        throw runtime_error("parse error - not all braces are closed");
    // and check that nothing is left unparsed - so people can't enter in garbage
    for(auto i = (semicolons.size() ? semicolons.back() + 1u : 0u); i < text.size(); ++i)
        if(!isspace(text[i]))
            throw runtime_error("parse error - unparsed contents");

    return out;
}

} // namespace rcrl
