#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "notverygl.h"
#include "vec3.h"

enum class ShaderMode {
    Random,
    Diffuse,
    PhongSpecular,
    BlinnPhongSpecular
};

constexpr int min_output_size = 800;
constexpr int max_output_size = 1920;

struct RenderSettings {
    int width = min_output_size;
    int height = min_output_size;

    std::vector<std::filesystem::path> model_paths;

    ViewMode view_mode = ViewMode::Shaded;
    ShaderMode shader_mode = ShaderMode::Diffuse;

    vec3 eye{-1, 0, 2};            // in world coordinates
    vec3 center{0, 0, 0};
    vec3 up{0, 1, 0};
    vec3 light_direction{1, 1, 1}; // in world coordinates

    double ambient = 0.4;
    double diffuse = 1.0;
    double specular = 1.0;
    double shininess = 35.0;

    std::filesystem::path output_path{"framebuffer.tga"};
};

struct RenderResult {
    bool succeeded = false;
    double duration_ms = 0.0;
    std::string error;
};

RenderResult render(const RenderSettings& settings);
RenderResult render_demo(const RenderSettings& settings);
