#include "host_app.h"
#include "TextEditor.h"
#include "process.hpp"

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
#define CopyDynlib(src, dst) (!system((string("cp ") + RCRL_BIN_FOLDER + src + " " + RCRL_BIN_FOLDER + dst).c_str()))

#define PLUGIN_EXTENSION ".so"
#define FILESYSTEM_SLASH "/"

#endif // _WIN32

#include <imgui.h>
#include <examples/opengl2_example/imgui_impl_glfw.h>
#include <GLFW/glfw3.h>

void f() { cout << "forced print!" << endl; }

void reconstruct_plugin_source_file() {
    ofstream myfile(RCRL_PLUGIN_FILE);
    myfile << "#include \"host_app.h\"\n";
    myfile.close();
}

// my own callback - need to add the carriage return symbols to make ImGuiColorTextEdit work when 'enter' is pressed
void RCRL_ImGui_ImplGlfwGL2_KeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    // call the callback from the imgui/glfw integration
    ImGui_ImplGlfwGL2_KeyCallback(w, key, scancode, action, mods);

    ImGuiIO& io = ImGui::GetIO();
    if(key == GLFW_KEY_ENTER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        io.AddInputCharacter((unsigned short)'\r');
}

int main() {
    // Setup window
    glfwSetErrorCallback([](int error, const char* description) { cerr << error << " " << description << endl; });

    if(!glfwInit())
        return 1;
    GLFWwindow* window = glfwCreateWindow(1366, 768, "Read-Compile-Run-Loop - REPL for C++", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // Setup ImGui binding
    ImGui_ImplGlfwGL2_Init(window, true);

    // overwrite with my own callback
    glfwSetKeyCallback(window, RCRL_ImGui_ImplGlfwGL2_KeyCallback);

    // init the plugin file
    reconstruct_plugin_source_file();

    vector<pair<string, DynamicLib>> plugins;

    // an editor instance - for the already submitted code
    TextEditor history;
    history.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    history.SetReadOnly(true);

    // an editor instance - for the core being currently written
    TextEditor editor;
    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());

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
            const auto text_field_height = ImGui::GetTextLineHeight() * 20;
            ImGui::BeginChild("history code", ImVec2(700, text_field_height));
            history.Render("History");
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("compiler errors", ImVec2(0, text_field_height));
            ImGui::InputTextMultiline("##errors", statuses.data(), statuses.size(), ImVec2(-1.0f, text_field_height),
                                      ImGuiInputTextFlags_ReadOnly);
            ImGui::EndChild();

            ImGui::BeginChild("source code", ImVec2(-1.f, text_field_height));
            editor.Render("Code");
            ImGui::EndChild();

            ImGuiIO& io = ImGui::GetIO();
            if((io.KeysDown[GLFW_KEY_ENTER] && io.KeyCtrl) &&
               (editor.GetTotalLines() > 1 || editor.GetText().size() > 1)) {
                auto code = editor.GetText();

                history.SetText(history.GetText() + code);
                history.SetCursorPosition({history.GetTotalLines(), 1});

                // append to file
                ofstream myfile(RCRL_PLUGIN_FILE, ios::out | ios::app);
                myfile << code << endl;
                myfile.close();

                // rebuild the plugin
                int res = system("cmake --build " RCRL_BUILD_FOLDER " --target plugin"
#ifdef RCRL_CONFIG
                                 " --config " RCRL_CONFIG
#endif // multi config IDE
#if defined(RCRL_CONFIG) && defined(_MSC_VER)
                                 " -- /verbosity:quiet /consoleloggerparameters:PerformanceSummary"
#endif // Visual Studio
                );

                if(res) {
                    // errors occurred
                    statuses += "ERRORED\n";
                } else {
                    statuses += "SUCCESS\n";

                    // clear the editor
                    editor.SetText("\r"); // an empty string "" breaks it for some reason...
                    editor.SetCursorPosition({0, 0});

                    // copy the plugin
                    auto plugin_name =
                            string(RCRL_BIN_FOLDER) + "plugin_" + to_string(plugins.size()) + PLUGIN_EXTENSION;
                    const auto copy_res = CopyDynlib((string(RCRL_BIN_FOLDER) + "plugin" PLUGIN_EXTENSION).c_str(),
                                                     plugin_name.c_str());

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
        system((string("del ") + RCRL_BIN_FOLDER + "plugin_*" PLUGIN_EXTENSION " /Q").c_str());
    plugins.clear();

    return 0;
}
