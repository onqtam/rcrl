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
#define CopyDynlib(src, dst)                                                                                           \
    (!system((string("cp ") + get_path_to_exe() + src + " " + get_path_to_exe() + dst).c_str()))

#define PLUGIN_EXTENSION ".so"
#define FILESYSTEM_SLASH "/"

#endif // _WIN32

#include <imgui.h>
#include <examples/opengl2_example/imgui_impl_glfw.h>
#include <GLFW/glfw3.h>

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
    // Setup window
    glfwSetErrorCallback([](int error, const char* description) { cerr << error << " " << description << endl; });

    if(!glfwInit())
        return 1;
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Read-Compile-Run-Loop - REPL for C++", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // Setup ImGui binding
    ImGui_ImplGlfwGL2_Init(window, true);

    // init the plugin file
    reconstruct_plugin_source_file();

    vector<pair<string, DynamicLib>> plugins;

    char text[1024 * 16] = "";

	string code;
	string statuses;

    // Main loop
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplGlfwGL2_NewFrame();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        ImGui::SetNextWindowSize({(float)display_w, -1.f}, ImGuiCond_Always);
        ImGui::SetNextWindowPos({0.f, 0.f}, ImGuiCond_Always);

        auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

        if(ImGui::Begin("console", nullptr, flags)) {
            static bool just_submitted                     = false;
            static bool just_submitted_2nd_pass_for_scroll = false;

            const auto text_field_height = ImGui::GetTextLineHeight() * 16;
            ImGui::BeginChild("source code", ImVec2(700, text_field_height));
            ImGui::InputTextMultiline("##source", code.data(), code.size(), ImVec2(-1.0f, text_field_height),
                                      ImGuiInputTextFlags_ReadOnly);
            if(just_submitted_2nd_pass_for_scroll) {
                ImGui::BeginChild(ImGui::GetID("##source"));
                ImGui::SetScrollY(ImGui::GetScrollMaxY());
                ImGui::EndChild();
                just_submitted_2nd_pass_for_scroll = false;
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("compiler errors", ImVec2(0, text_field_height));
            ImGui::InputTextMultiline("##errors", statuses.data(), statuses.size(), ImVec2(-1.0f, text_field_height),
                                      ImGuiInputTextFlags_ReadOnly);
            ImGui::EndChild();

            if(just_submitted) {
                ImGui::SetKeyboardFocusHere();
                just_submitted_2nd_pass_for_scroll = true;
            }
            just_submitted =
                    ImGui::InputTextMultiline("##current", text, IM_ARRAYSIZE(text), ImVec2(-1.0f, text_field_height),
                                              ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AllowTabInput);

            // submit
            if(just_submitted) {
                auto len = strlen(text);
                code += text;
                if(text[len - 1] != '\n')
                    code += '\n';

                // append to file
                ofstream myfile(CRCL_PLUGIN_FILE, ios::out | ios::app);
                myfile << text << endl;
                myfile.close();

                // rebuild the plugin
                int res = system("cmake --build " CRCL_BUILD_FOLDER " --target plugin " CRCL_ADDITIONAL_FLAGS);

                if(res) {
                    // errors occurred
					statuses += "ERRORED\n";
                } else {
					statuses += "SUCCESS\n";

                    // clear the entered text
                    text[0] = '\0';

                    // copy the plugin
                    auto plugin_name = get_path_to_exe() + "plugin_" + to_string(plugins.size()) + PLUGIN_EXTENSION;
                    const auto copy_res =
                            CopyDynlib((get_path_to_exe() + "plugin" PLUGIN_EXTENSION).c_str(), plugin_name.c_str());

                    assert(copy_res);

                    // load the plugin
                    auto plugin = LoadDynlib(plugin_name.c_str());
                    assert(plugin);

                    // add the plugin to the list of loaded ones - for later unloading
                    plugins.push_back({plugin_name, plugin});
                }
            }
        }
        ImGui::End();

        //ImGui::ShowDemoWindow();

        // Rendering
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        // draw a triangle
        {
            glBegin(GL_TRIANGLES);

            glColor3f(0.5, 0, 0);

            glVertex2f(0.f, 1.f);
            glVertex2f(-1.f, -1.f);
            glVertex2f(1.f, -1.f);

            glEnd();
        }

        ImGui::Render();
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplGlfwGL2_Shutdown();
    glfwTerminate();

    // cleanup plugins
    for(auto plugin : plugins)
        CloseDynlib(plugin.second);

    if(plugins.size())
        system((string("del ") + get_path_to_exe() + "plugin_*" PLUGIN_EXTENSION " /Q").c_str());
    plugins.clear();

    return 0;
}
