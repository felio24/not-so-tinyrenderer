#include <GLFW/glfw3.h>
#include <iostream>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "gui.h"

namespace {
    void draw_render_controls() {
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);

        ImGui::Begin("Render Controls", nullptr, ImGuiWindowFlags_NoResize);
        ImGui::TextUnformatted("Tweak me!");

        ImGui::SeparatorText("Scene");
        ImGui::TextUnformatted("Model folder: -");
        ImGui::TextUnformatted("Models: -");

        ImGui::SeparatorText("View");
        ImGui::TextUnformatted("View mode: Shaded");
        ImGui::TextUnformatted("Lighting model: Diffuse");

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

        ImGui::SeparatorText("Output");
        ImGui::TextUnformatted("Size: 800 x 800");
        ImGui::TextUnformatted("File: framebuffer.tga");
        ImGui::Spacing();

        constexpr float render_button_width = 100.0f;
        const float right_alignment_offset = ImGui::GetContentRegionAvail().x - render_button_width;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + right_alignment_offset);
        ImGui::Button("Render >:)", ImVec2(render_button_width, 0));

        ImGui::End();
    }
}

int launch_gui() {
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

        draw_render_controls();

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
