#include "host_app.h"
#include "rcrl/rcrl.h"

#include <GLFW/glfw3.h>
#include <third_party/ImGuiColorTextEdit/TextEditor.h>
#include <third_party/imgui/examples/opengl2_example/imgui_impl_glfw.h>

using namespace std;

//bool g_console_visible = true;

// my own callback - need to add the carriage return symbols to make ImGuiColorTextEdit work when 'enter' is pressed
void My_ImGui_ImplGlfwGL2_KeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    // call the callback from the imgui/glfw integration
    ImGui_ImplGlfwGL2_KeyCallback(w, key, scancode, action, mods);

    // add the '\n' char when 'enter' is pressed - for ImGuiColorTextEdit
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
    GLFWwindow* window = glfwCreateWindow(1280, 1024, "Read-Compile-Run-Loop - REPL for C++", nullptr, nullptr);
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
    history.SetText("#include \"precompiled_for_plugin.h\"\n");

    // an editor instance - for the core being currently written
    TextEditor editor;
    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());

    // compiler output - with an empty language definition and a custom palette
    TextEditor compiler_output;
    compiler_output.SetLanguageDefinition(TextEditor::LanguageDefinition());
    compiler_output.SetReadOnly(true);
    compiler_output.SetPalette({
            0xcccccccc, // None
            0xcccccccc, // Keyword
            0xcccccccc, // Number
            0xcccccccc, // String
            0xcccccccc, // Char literal
            0xcccccccc, // Punctuation
            0xcccccccc, // Preprocessor
            0xcccccccc, // Identifier
            0xcccccccc, // Known identifier
            0xcccccccc, // Preproc identifier
            0xcccccccc, // Comment (single line)
            0xcccccccc, // Comment (multi line)
            0xff101010, // Background
            0xcccccccc, // Cursor
            0x80a06020, // Selection
            0xcccccccc, // ErrorMarker
            0xcccccccc, // Breakpoint
            0xcccccccc, // Line number
            0x40000000, // Current line fill
            0x40808080, // Current line fill (inactive)
            0x40a0a0a0, // Current line edge
    });

    // set initial code
    editor.SetText(
            R"raw(cout << "hello!\n";
// global
int f() { return 42; }
// vars
int a = f();
// once
a++;
// once
cout << a << endl;
)raw");

    // holds the standard output from while loading the plugin
    string redirected_stdout;

    // holds the exit code from the last compilation - there was an error when not 0
    int last_compiler_exitcode = 0;

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
            ImGui::BeginChild("history code", ImVec2(display_w * 0.45f, text_field_height));
            auto hcpos = editor.GetCursorPosition();
            ImGui::Text("Executed code: %3d/%-3d %3d lines", hcpos.mLine + 1, hcpos.mColumn + 1, editor.GetTotalLines());
            history.Render("History");
            ImGui::EndChild();
            ImGui::SameLine();
            // top right part
            ImGui::BeginChild("compiler output", ImVec2(0, text_field_height));
            auto new_output = rcrl::get_new_compiler_output();
            if(new_output.size()) {
                compiler_output.SetText(compiler_output.GetText() + new_output);
                compiler_output.SetCursorPosition({compiler_output.GetTotalLines(), 1});
            }
            if(last_compiler_exitcode)
                ImGui::TextColored({1, 0, 0, 1}, "Compiler output - ERROR!");
            else
                ImGui::Text("Compiler output:        ");
            ImGui::SameLine();
            auto cocpos = compiler_output.GetCursorPosition();
            ImGui::Text("%3d/%-3d %3d lines", cocpos.mLine + 1, cocpos.mColumn + 1, compiler_output.GetTotalLines());
            compiler_output.Render("Compiler output");
            ImGui::EndChild();

            // bottom left part
            ImGui::BeginChild("source code", ImVec2(display_w * 0.45f, text_field_height));
            auto ecpos = editor.GetCursorPosition();
            ImGui::Text("RCRL Console: %3d/%-3d %3d lines | %s", ecpos.mLine + 1, ecpos.mColumn + 1, editor.GetTotalLines(),
                        editor.CanUndo() ? "*" : " ");
            editor.Render("Code");
            ImGui::EndChild();
            ImGui::SameLine();
            // bottom right part
            ImGui::BeginChild("program output", ImVec2(0, text_field_height));
            ImGui::Text("Program output");
            ImGui::InputTextMultiline("##program_output", (char*)redirected_stdout.data(), redirected_stdout.size(),
                                      ImVec2(-1.f, -1.f), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndChild();

            static rcrl::Mode mode = rcrl::ONCE;
            ImGui::Text("Default mode:");
            ImGui::SameLine();
            ImGui::RadioButton("global", (int*)&mode, rcrl::GLOBAL);
            ImGui::SameLine();
            ImGui::RadioButton("vars", (int*)&mode, rcrl::VARS);
            ImGui::SameLine();
            ImGui::RadioButton("once", (int*)&mode, rcrl::ONCE);
            ImGui::SameLine();
            auto compile = ImGui::Button("Compile and run");
            ImGui::SameLine();
            if(ImGui::Button("Cleanup") && !rcrl::is_compiling()) {
                rcrl::cleanup_plugins();
                compiler_output.SetText("");
                redirected_stdout.clear();
                history.SetText("#include \"precompiled_for_plugin.h\"\n");
            }

            // if the user has submitted code
            ImGuiIO& io = ImGui::GetIO();
            compile |= (io.KeysDown[GLFW_KEY_ENTER] && io.KeyCtrl);
            if(compile && !rcrl::is_compiling() && editor.GetText().size() > 1) {
                // clear compiler output
                compiler_output.SetText("");

                // submit to the RCRL engine
                if(rcrl::submit_code(editor.GetText(), mode)) {
                    // make the editor code untouchable while compiling
                    editor.SetReadOnly(true);
                } else {
                    last_compiler_exitcode = 1;

                    TextEditor::ErrorMarkers markers;
                    markers.insert(std::make_pair<int, std::string>(
                            6, "Example error here:\nInclude file not found: \"TextEditor.h\""));
                    markers.insert(std::make_pair<int, std::string>(41, "Another example error"));
                    editor.SetErrorMarkers(markers);
                }
            }
        }
        ImGui::End();
        //}

        // if there is a spawned compiler process and it has just finished
        if(rcrl::try_get_exit_status_from_compile(last_compiler_exitcode)) {
            // we can edit the code again
            editor.SetReadOnly(false);

            if(last_compiler_exitcode) {
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
                redirected_stdout += rcrl::copy_and_load_new_plugin(true);
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
