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

#include <GLFW/glfw3.h>
#include <third_party/tiny-process-library/process.hpp>
#include <third_party/ImGuiColorTextEdit/TextEditor.h>
#include <third_party/imgui/examples/opengl2_example/imgui_impl_glfw.h>

#include <iostream>
#include <fstream>

#include "host_app.h"

using namespace std;

vector<pair<string, RCRL_Dynlib>> plugins;

void cleanup_plugins() {
    for(auto it = plugins.rbegin(); it != plugins.rend(); ++it)
        RCRL_CloseDynlib(it->second);

    if(plugins.size())
        system((string("del ") + RCRL_BIN_FOLDER + "plugin_*" RCRL_EXTENSION " /Q").c_str());
    plugins.clear();
}

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
    GLFWwindow* window = glfwCreateWindow(1024, 768, "Read-Compile-Run-Loop - REPL for C++", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // Setup ImGui binding
    ImGui_ImplGlfwGL2_Init(window, true);

    // overwrite with my own callback
    glfwSetKeyCallback(window, RCRL_ImGui_ImplGlfwGL2_KeyCallback);

    // init the plugin file
    reconstruct_plugin_source_file();

    // an editor instance - for the already submitted code
    TextEditor history;
    history.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    history.SetReadOnly(true);

    // an editor instance - for the core being currently written
    TextEditor editor;
    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());

    unique_ptr<TinyProcessLib::Process> compiler_process;
    string                              compiler_output;
    auto output_appender = [&](const char* bytes, size_t n) { compiler_output += string(bytes, n); };

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
            ImGui::BeginChild("history code", ImVec2(display_w * 0.5f, text_field_height));
            history.Render("History");
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("compiler output", ImVec2(0, text_field_height));
            ImGui::InputTextMultiline("##compiler_output", compiler_output.data(), compiler_output.size(),
                                      ImVec2(-1.0f, text_field_height), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndChild();

            ImGui::BeginChild("source code", ImVec2(display_w * 0.5f, text_field_height));
            editor.Render("Code");
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("program output", ImVec2(0, text_field_height));
            ImGui::InputTextMultiline("##program_output", compiler_output.data(), compiler_output.size(),
                                      ImVec2(-1.0f, text_field_height), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndChild();

            ImGuiIO& io = ImGui::GetIO();
            if((io.KeysDown[GLFW_KEY_ENTER] && io.KeyCtrl) && (editor.GetTotalLines() > 1 || editor.GetText().size() > 1) &&
               !compiler_process) {
                history.SetCursorPosition({history.GetTotalLines(), 1});

                // append to file
                ofstream myfile(RCRL_PLUGIN_FILE, ios::out | ios::app);
                myfile << editor.GetText() << endl;
                myfile.close();

                // make the editor code untouchable while compiling
                editor.SetReadOnly(true);

                compiler_output.clear();
                compiler_process    = make_unique<TinyProcessLib::Process>(
                        "cmake --build " RCRL_BUILD_FOLDER " --target plugin"
#ifdef RCRL_CONFIG
                        " --config " RCRL_CONFIG
#endif // multi config IDE
#if defined(RCRL_CONFIG) && defined(_MSC_VER)
                        " -- /verbosity:quiet /consoleloggerparameters:PerformanceSummary"
#endif // Visual Studio
                        ,
                        "", output_appender, output_appender);
            }
        }
        ImGui::End();

        if(int exitcode = 0; compiler_process && compiler_process->try_get_exit_status(exitcode)) {
            // we can edit the code again
            editor.SetReadOnly(false);

            compiler_process.reset();

            if(exitcode) {
                // errors occurred
                editor.SetCursorPosition({editor.GetTotalLines(), 1});
            } else {
                // append to the history
                history.SetText(history.GetText() + editor.GetText());

                // clear the editor
                editor.SetText("\r"); // an empty string "" breaks it for some reason...
                editor.SetCursorPosition({0, 0});

                // copy the plugin
                auto       name_copied = string(RCRL_BIN_FOLDER) + "plugin_" + to_string(plugins.size()) + RCRL_EXTENSION;
                const auto copy_res =
                        RCRL_CopyDynlib((string(RCRL_BIN_FOLDER) + "plugin" RCRL_EXTENSION).c_str(), name_copied.c_str());
                assert(copy_res);

                // load the plugin
                auto plugin = RDRL_LoadDynlib(name_copied.c_str());
                assert(plugin);

                // add the plugin to the list of loaded ones - for later unloading
                plugins.push_back({name_copied, plugin});
            }
        }

        //ImGui::ShowDemoWindow();

        // Rendering
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

		draw();

        ImGui::Render();
        glfwSwapBuffers(window);
    }

    // Cleanup
    cleanup_plugins();
    ImGui_ImplGlfwGL2_Shutdown();
    glfwTerminate();

    return 0;
}
