#include <chrono>
#include <thread>

#include <GLFW/glfw3.h>
#include <third_party/ImGuiColorTextEdit/TextEditor.h>
#include <third_party/imgui/examples/opengl2_example/imgui_impl_glfw.h>

#include "host_app.h"
#include "rcrl/rcrl.h"

using namespace std;

bool g_console_visible = true;

// my own callback - need to add the new line symbols to make ImGuiColorTextEdit work when 'enter' is pressed
void My_ImGui_ImplGlfwGL2_KeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    // calling the callback from the imgui/glfw integration only if not a dash because when writing an underscore (with shift down)
    // ImGuiColorTextEdit does a paste - see this for more info: https://github.com/BalazsJako/ImGuiColorTextEdit/issues/18
    if(key != '-')
        ImGui_ImplGlfwGL2_KeyCallback(w, key, scancode, action, mods);

    // add the '\n' char when 'enter' is pressed - for ImGuiColorTextEdit
    ImGuiIO& io = ImGui::GetIO();
    if(key == GLFW_KEY_ENTER && !io.KeyCtrl && (action == GLFW_PRESS || action == GLFW_REPEAT))
        io.AddInputCharacter((unsigned short)'\n');

    // console toggle
    if(!io.WantCaptureKeyboard && !io.WantTextInput && key == GLFW_KEY_GRAVE_ACCENT &&
       (action == GLFW_PRESS || action == GLFW_REPEAT))
        g_console_visible = !g_console_visible;
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
    // this is the precompiled header for the plugin in this demo project so it's contents are always there for the plugin
    history.SetText("#include \"precompiled_for_plugin.h\"\n");

    // an editor instance - compiler output - with an empty language definition and a custom palette
    TextEditor compiler_output;
    compiler_output.SetLanguageDefinition(TextEditor::LanguageDefinition());
    compiler_output.SetReadOnly(true);
    auto custom_palette                                             = TextEditor::GetDarkPalette();
    custom_palette[(int)TextEditor::PaletteIndex::MultiLineComment] = 0xcccccccc; // text is treated as such by default
    compiler_output.SetPalette(custom_palette);

    // an editor instance - for the core being currently written
    TextEditor editor;
    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());

    // set some initial code
    editor.SetText(R"raw(// global
#include "host_app.h"
// once
cout << "Hello!" << endl;
)raw");

    // holds the standard output from while loading the plugin
    string redirected_stdout;

    // holds the exit code from the last compilation - there was an error when not 0
    int last_compiler_exitcode = 0;

    // if the default mode was used for the first section
    bool       used_default_mode = false;
    rcrl::Mode default_mode      = rcrl::ONCE;

    // limiting to 50 fps because on some systems the whole machine started lagging when the demo was turned on
    using frames   = chrono::duration<int64_t, ratio<1, 50>>;
    auto nextFrame = chrono::system_clock::now();

    // add objects in scene
    auto& objects = getObjects();
    for(int i = 0; i < 4; ++i) {
        for(int k = 0; k < 4; ++k) {
            auto& obj = addObject(-7.f + k * 5, -6.f + i * 4);
            obj.colorize(float(i % 2), float(k % 2), 0);
        }
    }

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

        if(g_console_visible &&
           ImGui::Begin("console", nullptr,
                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            const auto text_field_height = ImGui::GetTextLineHeight() * 16;
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
                auto total_output = compiler_output.GetText() + new_output;

                // scan for errors through the lines and highlight them with markers
                auto                     curr_pos                = 0;
                auto                     line                    = 1;
                auto                     first_error_marker_line = 0;
                TextEditor::ErrorMarkers error_markers;
                do {
                    auto new_curr_pos_1 = total_output.find("error", curr_pos + 1); // add 1 to skip new lines
                    auto new_curr_pos_2 = total_output.find("\n", curr_pos + 1);    // add 1 to skip new lines
                    if(new_curr_pos_1 < new_curr_pos_2) {
                        error_markers.insert(make_pair(line, ""));
                        if(!first_error_marker_line)
                            first_error_marker_line = line;
                    }
                    if(new_curr_pos_2 < new_curr_pos_1) {
                        line++;
                    }
                    curr_pos = min(new_curr_pos_1, new_curr_pos_2);
                } while(curr_pos != string::npos);
                compiler_output.SetErrorMarkers(error_markers);

                // update compiler output
                compiler_output.SetText(move(total_output));
                if(first_error_marker_line)
                    compiler_output.SetCursorPosition({first_error_marker_line, 1});
                else
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

            // bottom buttons
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
                last_compiler_exitcode = 0;
                history.SetText("#include \"precompiled_for_plugin.h\"\n");
            }
            ImGui::SameLine();
            ImGui::Dummy({20, 0});
            ImGui::SameLine();
            ImGui::Text("Use Ctrl+Enter to submit code");

            // if the user has submitted code for compilation
            ImGuiIO& io = ImGui::GetIO();
            compile |= (io.KeysDown[GLFW_KEY_ENTER] && io.KeyCtrl);
            if(compile && !rcrl::is_compiling() && editor.GetText().size() > 1) {
                // clear compiler output
                compiler_output.SetText("");

                // submit to the RCRL engine
                if(rcrl::submit_code(editor.GetText(), mode, &used_default_mode)) {
                    default_mode = mode;
                    // make the editor code untouchable while compiling
                    editor.SetReadOnly(true);
                } else {
                    last_compiler_exitcode = 1;
                }
            }
            ImGui::End();
        }

        // if there is a spawned compiler process and it has just finished
        if(rcrl::try_get_exit_status_from_compile(last_compiler_exitcode)) {
            // we can edit the code again
            editor.SetReadOnly(false);

            if(last_compiler_exitcode) {
                // errors occurred - set cursor to the last line of the erroneous code
                editor.SetCursorPosition({editor.GetTotalLines(), 0});
            } else {
                // append to the history and focus last line
                history.SetCursorPosition({history.GetTotalLines(), 1});
                auto history_text = history.GetText();
                // add a new line (if one is missing) to the code that will go to the history for readability
                if(history_text.size() && history_text.back() != '\n')
                    history_text += '\n';
                // if the default mode was used - add an extra comment before the code to the history for clarity
                if(used_default_mode)
                    history_text += default_mode == rcrl::GLOBAL ? "// global\n" :
                                                                   (default_mode == rcrl::VARS ? "// vars\n" : "// once\n");
                history.SetText(history_text + editor.GetText());

                // clear the editor
                editor.SetText("\r"); // an empty string "" breaks it for some reason...
                editor.SetCursorPosition({0, 0});

                // load the new plugin
                redirected_stdout += rcrl::copy_and_load_new_plugin(true);
            }
        }

        // rendering
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        glPushMatrix();
        glLoadIdentity();
        glScalef(0.1, 0.1 * display_w / display_h, 0.1);

        for(auto& obj : getObjects())
            obj.draw();

        glPopMatrix();

        // finalize rendering
        ImGui::Render();
        glfwSwapBuffers(window);

        // do the frame rate limiting
        this_thread::sleep_until(nextFrame);
        nextFrame += frames{1};
    }

    // cleanup
    rcrl::cleanup_plugins();
    ImGui_ImplGlfwGL2_Shutdown();
    glfwTerminate();

    return 0;
}
