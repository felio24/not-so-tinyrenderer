#include <algorithm>
#include <array>
#include <GLFW/glfw3.h>
#include <iostream>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "modelgroup.h"
#include "renderer.h"

#include "gui.h"

namespace {
    struct ViewModeOption {
        ViewMode value;
        const char* label;
    };

    constexpr std::array view_mode_options{
        ViewModeOption{ViewMode::Shaded, "Shaded"},
        ViewModeOption{ViewMode::Flat, "Flat"},
        ViewModeOption{ViewMode::Wireframe, "Wireframe"},
        ViewModeOption{ViewMode::Depth, "Depth"},
        ViewModeOption{ViewMode::Barycentric, "Barycentric"},
    };

    struct ShaderModeOption {
        ShaderMode shader;
        const char* label;
    };

    constexpr std::array lighting_mode_options{
        ShaderModeOption{ShaderMode::Diffuse, "Diffuse"},
        ShaderModeOption{ShaderMode::PhongSpecular, "Phong"},
        ShaderModeOption{ShaderMode::BlinnPhongSpecular, "Blinn-Phong"},
        // ShaderModeOption{ShaderMode::Random, "Random"}, TODO: Implement Random shader
    };

    struct GuiState {
        RenderSettings settings;

        std::vector<ModelGroup> model_groups;
        std::size_t selected_group = 0;
        std::vector<bool> selected_models;

        RenderResult last_result;
        bool has_last_result = false;
        std::string validation_error;
    };

    void draw_render_controls_scene_selection(GuiState& state) {
        std::vector<std::string> items;

        ImGui::SeparatorText("Scene");
        if (state.model_groups.empty()) {
            state.validation_error = "No OBJ models found in obj/";
            ImGui::TextDisabled("%s", state.validation_error.c_str());
            return;
        }

        for (const ModelGroup& models : state.model_groups)
        {
            items.emplace_back(models.name);
        }

        constexpr float label_width = 120.0f;

        static ImGuiComboFlags flags = 0;
        const char* preview = items.empty()
            ? "No models found"
            : items[state.selected_group].c_str();

        if (ImGui::BeginTable("scene_fields", 2)) {
            ImGui::TableSetupColumn(
                "Label",
                ImGuiTableColumnFlags_WidthFixed,
                label_width
            );
            ImGui::TableSetupColumn(
                "Field",
                ImGuiTableColumnFlags_WidthStretch
            );

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Model folder:");

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##model_folder", preview, flags)) {
                for (int n = 0; n < items.size(); n++)
                {
                    const bool is_selected = (state.selected_group == n);
                    if (ImGui::Selectable(items[n].c_str(), is_selected)) {
                        state.selected_group = n;
                        state.selected_models.assign(
                            state.model_groups[n].models.size(),
                            false
                        );
                        state.settings.model_paths.clear();
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Models:");

            // Loop through selected model
            const ModelGroup& selected_group = state.model_groups[state.selected_group];
            const std::vector<std::filesystem::path>& models =
                selected_group.models;

            bool selection_changed = false;
            for (std::size_t index = 0; index < models.size(); ++index) {
                const std::string path_id = models[index].string();
                const std::string filename = models[index].filename().string();

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

                bool selected = state.selected_models[index];
                ImGui::PushID(path_id.c_str());
                if (ImGui::Checkbox(filename.c_str(), &selected)) {
                    state.selected_models[index] = selected;
                    selection_changed = true;
                }
                ImGui::PopID();

                ImGui::TableNextRow();
            }

            if (selection_changed) {
                state.settings.model_paths.clear();

                for (std::size_t index = 0; index < models.size(); ++index) {
                    if (state.selected_models[index]) {
                        state.settings.model_paths.push_back(models[index]);
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    const char* view_mode_label(ViewMode mode) {
        for (const ViewModeOption& option : view_mode_options) {
            if (option.value == mode) {
                return option.label;
            }
        }

        return "Unknown";
    }

    const char* lighting_mode_label(ShaderMode mode) {
        for (const ShaderModeOption& option : lighting_mode_options) {
            if (option.shader == mode) {
                return option.label;
            }
        }

        return "Unknown";
    }

    void draw_render_controls_view_selection(GuiState& state) {
        std::vector<std::string> items = {};

        ImGui::SeparatorText("View");

        constexpr float label_width = 120.0f;

        if (ImGui::BeginTable("view_fields", 2)) {
            ImGui::TableSetupColumn(
                "Label",
                ImGuiTableColumnFlags_WidthFixed,
                label_width
            );
            ImGui::TableSetupColumn(
                "Field",
                ImGuiTableColumnFlags_WidthStretch
            );

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("View mode:");

            const char* view_mode_preview = view_mode_label(state.settings.view_mode);

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##view_folder", view_mode_preview)) {
                for (const ViewModeOption& option : view_mode_options) {
                    const bool selected =
                        state.settings.view_mode == option.value;

                    if (ImGui::Selectable(option.label, selected)) {
                        state.settings.view_mode = option.value;
                    }

                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const bool shaded = state.settings.view_mode == ViewMode::Shaded;
            ImGui::BeginDisabled(!shaded);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Lighting model:");

            const char* lighting_mode_preview = lighting_mode_label(state.settings.shader_mode);

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##lighting_folder", lighting_mode_preview)) {
                for (const ShaderModeOption& option : lighting_mode_options) {
                    const bool selected =
                        state.settings.shader_mode == option.shader;

                    if (ImGui::Selectable(option.label, selected)) {
                        state.settings.shader_mode = option.shader;
                    }

                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::EndDisabled();

            ImGui::EndTable();
        }
    }

    void draw_render_controls_output(GuiState& state) {
        ImGui::SeparatorText("Output");

        if (ImGui::BeginTable("output_controls_fields", 2)) {
            constexpr float label_width = 120.0f;

            ImGui::TableSetupColumn(
                "Label",
                ImGuiTableColumnFlags_WidthFixed,
                label_width
            );
            ImGui::TableSetupColumn(
                "Field",
                ImGuiTableColumnFlags_WidthStretch
            );

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Width:");

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            ImGui::InputInt("##width", &state.settings.width);
            state.settings.width = std::clamp(state.settings.width, min_output_size, max_output_size);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Height:");

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            ImGui::InputInt("##height", &state.settings.height);
            state.settings.height = std::clamp(state.settings.height, min_output_size, max_output_size);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("File:");

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            static char output_path[256] = "framebuffer.tga";
            if (ImGui::InputTextWithHint("##output_path", "framebuffer.tga", output_path, IM_ARRAYSIZE(output_path))) {
                state.settings.output_path = output_path;
            };

            ImGui::EndTable();
        }
    }

    void draw_render_controls_output_message_and_button(GuiState& state) {
        if (ImGui::BeginTable("output_fields", 2)) {
            constexpr float render_button_width = 115.0f;
            ImGui::TableSetupColumn(
                "Output message",
                ImGuiTableColumnFlags_WidthStretch
            );
            ImGui::TableSetupColumn(
                "Render button",
                ImGuiTableColumnFlags_WidthFixed,
                render_button_width
            );

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();

            if (state.has_last_result) {
                if (state.last_result.succeeded) {
                    ImGui::TextColored(
                        ImVec4(0.25f, 0.85f, 0.35f, 1.0f),
                        "Render time: %.2f ms",
                        state.last_result.duration_ms
                    );
                } else {
                    ImGui::TextColored(
                        ImVec4(0.95f, 0.25f, 0.25f, 1.0f),
                        "Error: %s",
                        state.last_result.error.c_str()
                    );
                }
            }

            ImGui::TableSetColumnIndex(1);
            if (ImGui::Button("Render >:)", ImVec2(render_button_width, 0))) {
                RenderSettings request = state.settings;
                RenderResult result{false, 0.0, "No OBJ selected :<"}; // Default result
                if (!request.model_paths.empty()) {
                    result = render(request);
                }

                if (!result.succeeded) {
                    std::cerr << "Rendering failed with message: " << result.error << '\n';
                } else {
                    std::clog << "Render time: " << result.duration_ms << " ms\n";
                }

                state.last_result = result;
                state.has_last_result = true;
            }

            ImGui::EndTable();
        }
    }

    void draw_render_controls(GuiState& state) {
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);

        ImGui::Begin("Render Controls", nullptr, ImGuiWindowFlags_NoResize);
        ImGui::TextUnformatted("Tweak me!");

        draw_render_controls_scene_selection(state);
        draw_render_controls_view_selection(state);

        ImGui::SeparatorText("Camera");
        ImGui::TextUnformatted("Eye: -1.0, 0.0, 2.0");
        ImGui::TextUnformatted("Target: 0.0, 0.0, 0.0");
        ImGui::TextUnformatted("Up: 0.0, 1.0, 0.0");

        ImGui::SeparatorText("Lighting and Material");
        ImGui::TextUnformatted("Light direction: 1.0, 1.0, 1.0");
        ImGui::TextUnformatted("Ambient: 0.4");
        ImGui::TextUnformatted("Diffuse: 1.0");
        ImGui::TextUnformatted("Specular: 1.0");
        ImGui::TextUnformatted("Shininess: 35.0");

        draw_render_controls_output(state);
        ImGui::Spacing();
        draw_render_controls_output_message_and_button(state);

        ImGui::End();
    }
}

int launch_gui() {
    GuiState state;
    state.model_groups = discover_model_groups("obj");
    if (!state.model_groups.empty()) {
        state.selected_models.assign(
            state.model_groups[0].models.size(),
            false
        );
    }

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    #ifdef __APPLE__
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    float main_scale = 1.0f;
    if (GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor()) {
        float y_scale = 1.0f;
        glfwGetMonitorContentScale(primary_monitor, &main_scale, &y_scale);
    }

    GLFWwindow* window = glfwCreateWindow(
        static_cast<int>(600 * main_scale),
        static_cast<int>(750 * main_scale),
        "Not-So-Tiny Rasterizer :')",
        nullptr,
        nullptr
    );

    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_render_controls(state);

        ImGui::Render();

        int width;
        int height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
