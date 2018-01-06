#include "host_app.h"
#include "rcrl.h"

#include <GLFW/glfw3.h>
#include <third_party/ImGuiColorTextEdit/TextEditor.h>
#include <third_party/imgui/examples/opengl2_example/imgui_impl_glfw.h>

using namespace std;

//bool g_console_visible = true;

// my own callback - need to add the carriage return symbols to make ImGuiColorTextEdit work when 'enter' is pressed
void My_ImGui_ImplGlfwGL2_KeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    // call the callback from the imgui/glfw integration
    ImGui_ImplGlfwGL2_KeyCallback(w, key, scancode, action, mods);

    // add the '\r' char when 'enter' is pressed - for ImGuiColorTextEdit
    ImGuiIO& io = ImGui::GetIO();
    if(key == GLFW_KEY_ENTER && !io.KeyCtrl && (action == GLFW_PRESS || action == GLFW_REPEAT))
        io.AddInputCharacter((unsigned short)'\n');

    // this should be commented until this is fixed: https://github.com/BalazsJako/ImGuiColorTextEdit/issues/8
    //if(!io.WantCaptureKeyboard && !io.WantTextInput && key == GLFW_KEY_GRAVE_ACCENT &&
    //   (action == GLFW_PRESS || action == GLFW_REPEAT))
    //    g_console_visible = !g_console_visible;
}

int main() {
    // Setup window
    glfwSetErrorCallback([](int error, const char* description) { fprintf(stderr, "%d %s", error, description); });
    if(!glfwInit())
        return 1;
    GLFWwindow* window = glfwCreateWindow(1024, 768, "Read-Compile-Run-Loop - REPL for C++", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // Setup ImGui binding
    ImGui_ImplGlfwGL2_Init(window, true);

    // remove rounding of the console window
    ImGuiStyle& style    = ImGui::GetStyle();
    style.WindowRounding = 0.f;

    // overwrite with my own callback
    glfwSetKeyCallback(window, My_ImGui_ImplGlfwGL2_KeyCallback);

    // an editor instance - for the already submitted code
    TextEditor history;
    history.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    history.SetReadOnly(true);

    // an editor instance - for the core being currently written
    TextEditor editor;
    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());

    // holds the compiler output
    string compiler_output;

    // main loop
    while(!glfwWindowShouldClose(window)) {
        // poll for events
        glfwPollEvents();
        ImGui_ImplGlfwGL2_NewFrame();

        // handle window stretching
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        // console should be always fixed
        ImGui::SetNextWindowSize({(float)display_w, -1.f}, ImGuiCond_Always);
        ImGui::SetNextWindowPos({0.f, 0.f}, ImGuiCond_Always);

        auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

        //if(g_console_visible) {
        if(ImGui::Begin("console", nullptr, flags)) {
            const auto text_field_height = ImGui::GetTextLineHeight() * 15;
            // top left part
            ImGui::BeginChild("history code", ImVec2(display_w * 0.5f, text_field_height));
            auto hcpos = editor.GetCursorPosition();
            ImGui::Text("Executed code: %3d/%-3d %3d lines", hcpos.mLine + 1, hcpos.mColumn + 1, editor.GetTotalLines());
            history.Render("History");
            ImGui::EndChild();
            ImGui::SameLine();
            // top right part
            ImGui::BeginChild("compiler output", ImVec2(0, text_field_height));
            ImGui::Text("Compiler output");
            compiler_output += rcrl::get_new_compiler_output();
            ImGui::InputTextMultiline("##compiler_output", compiler_output.data(), compiler_output.size(),
                                      ImVec2(-1.f, -1.f), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndChild();

            // bottom left part
            ImGui::BeginChild("source code", ImVec2(display_w * 0.5f, text_field_height));
            auto ecpos = editor.GetCursorPosition();
            ImGui::Text("RCRL Console: %3d/%-3d %3d lines | %s", ecpos.mLine + 1, ecpos.mColumn + 1, editor.GetTotalLines(),
                        editor.CanUndo() ? "*" : " ");
            editor.Render("Code");
            ImGui::EndChild();
            ImGui::SameLine();
            // bottom right part
            ImGui::BeginChild("program output", ImVec2(0, text_field_height));
            static rcrl::Mode mode = rcrl::GLOBAL;
            ImGui::RadioButton("global", (int*)&mode, rcrl::GLOBAL);
            ImGui::RadioButton("vars", (int*)&mode, rcrl::VARS);
            ImGui::RadioButton("once", (int*)&mode, rcrl::ONCE);
            auto compile = ImGui::Button("Compile and run");
            if(ImGui::Button("Cleanup") && !rcrl::is_compiling()) {
                rcrl::cleanup_plugins();
				compiler_output.clear();
                history.SetText("\r");
            }
            //ImGui::Text("Program output");
            //ImGui::InputTextMultiline("##program_output", "", 0, ImVec2(-1.f, -1.f), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndChild();

            // if the user has submitted code
            ImGuiIO& io = ImGui::GetIO();
            compile |= (io.KeysDown[GLFW_KEY_ENTER] && io.KeyCtrl);
            if(compile && !rcrl::is_compiling() && editor.GetText().size() > 1) {
                // clear compiler output
                compiler_output.clear();

                // submit to the rcrl engine
				if (!rcrl::submit_code(editor.GetText(), mode)) {
					// make the editor code untouchable while compiling
					editor.SetReadOnly(true);
				}
            }
        }
        ImGui::End();
        //}

        // if there is a spawned compiler process and it has just finished
        int exitcode = 0;
        if(rcrl::try_get_exit_status_from_compile(exitcode)) {
            // we can edit the code again
            editor.SetReadOnly(false);

            if(exitcode) {
                // errors occurred
                editor.SetCursorPosition({editor.GetTotalLines(), 0});
            } else {
                // append to the history and focus last line
                history.SetCursorPosition({history.GetTotalLines(), 1});
                auto history_text = history.GetText();
                if(history_text.size() && history_text.back() != '\n')
                    history_text += '\n';
                history.SetText(history_text + editor.GetText());

                // clear the editor
                editor.SetText("\r"); // an empty string "" breaks it for some reason...
                editor.SetCursorPosition({0, 0});

                // load the new plugin
                rcrl::copy_and_load_new_plugin();
            }
        }

        //ImGui::ShowDemoWindow();

        // rendering
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        glPushMatrix();
        glLoadIdentity();
        static float frame = 0.f;
        frame += 1.f;
        glRotatef(frame, 0, 0, 1);
        draw();
        glPopMatrix();

        // finalize rendering
        ImGui::Render();
        glfwSwapBuffers(window);
    }

    // cleanup
    rcrl::cleanup_plugins();
    ImGui_ImplGlfwGL2_Shutdown();
    glfwTerminate();

    return 0;
}
