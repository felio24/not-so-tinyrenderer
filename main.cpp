#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "gui.h"
#include "mat.h"
#include "model.h"
#include "renderer.h"
#include "tgaimage.h"

#ifndef RASTERIZER_PARALLEL
#define RASTERIZER_PARALLEL 0
#endif

#if RASTERIZER_PARALLEL && defined(_OPENMP)
#include <omp.h>
#endif

constexpr int width  = 800;
constexpr int height = 800;

extern std::vector<double> zbuffer; // the depth buffer

std::optional<ViewMode> parse_view_mode(const std::string& mode) {
    if (mode == "shaded") return ViewMode::Shaded;
    if (mode == "flat") return ViewMode::Flat;
    if (mode == "intensity") return ViewMode::Depth;
    if (mode == "barycentric") return ViewMode::Barycentric;
    if (mode == "wireframe") return ViewMode::Wireframe;
    return std::nullopt;
}

std::optional<ShaderMode> parse_shader_mode(const std::string& mode) {
    if (mode == "random") return ShaderMode::Random;
    if (mode == "diffuse") return ShaderMode::Diffuse;
    if (mode == "phong") return ShaderMode::PhongSpecular;
    if (mode == "blinnphong") return ShaderMode::BlinnPhongSpecular;
    return std::nullopt;
}

TGAColor grayscale(unsigned char v) {
    return {{v, v, v, 255}, 4};
}

TGAImage depthbuffer_to_image(const std::vector<double>& depthbuffer) {
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const double z = depthbuffer[x + y * width];
            if (z == -std::numeric_limits<double>::infinity()) continue;

            const double shade = std::clamp((z + 1.0) * 255.0 / 2.0, 0.0, 255.0);
            const TGAColor color = {{static_cast<unsigned char>(shade), 0, 0, 0}, 1};
            zbuffer.set(x, y, color);
        }
    }
    return zbuffer;
}

void write_depthbuffer_text(const std::vector<double>& depthbuffer, const std::string& path) {
    std::ofstream out(path);
    out << std::fixed << std::setprecision(6);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x > 0) out << ' ';

            const double z = depthbuffer[x + y * width];
            if (z == -std::numeric_limits<double>::infinity()) {
                out << ".";
            } else {
                out << z;
            }
        }
        out << '\n';
    }
}

TGAColor barycentric_color(double u, double v, double w) {
    return {{static_cast<unsigned char>(u * 255),
            static_cast<unsigned char>(v * 255),
            static_cast<unsigned char>(w * 255),
            255}, 4};
}

TGAColor wireframe(double u, double v, double w) {
    double width = 0.11;
    double limit = 1 - width;
    if (u + v < limit && v + w < limit && u + w < limit) {
        return {{0,0,0,0},4};
    }

    return {{static_cast<unsigned char>(u * 255),
            static_cast<unsigned char>(v * 255),
            static_cast<unsigned char>(w * 255),
            255}, 4};
}


void lineShallow(int ax, int ay, int bx, int by, TGAImage& framebuffer, const TGAColor& color) {
    if (ax > bx) {
        std::clog << "Swapping points \n";
        std::swap(ax, bx);
        std::swap(ay, by);
    }

    const int dx = bx - ax;
    const int dy = by - ay;
    const int dir = dy < 0 ? -1 : 1;

    if (dx != 0) {
        const float m = static_cast<float>(dy) / static_cast<float>(dx);
        std::clog << "m: " << m << "\n";
        int y = ay;
        for (int i = 0; i <= dx; i++) {
            framebuffer.set(ax + i, y, color);
            //std::clog << "Colored " << ax + i << ", " << y << "\n";
            float py = m * (i + 1) + ay; // how much y has moved at x = i+1
            //std::clog << "At i: " << i << ", py: " << py << "\n";
            float d0 = py - y;
            float d1 = (y + dir) - py;
            if (std::abs(d0) - std::abs(d1) >= 0) {
                y += dir;
            }
        }
    }
}

void lineSteep(int ax, int ay, int bx, int by, TGAImage& framebuffer, const TGAColor& color) {
    if (ay > by) {
        std::clog << "Swapping points \n";
        std::swap(ax, bx);
        std::swap(ay, by);
    }

    const int dx = bx - ax;
    const int dy = by - ay;
    const int dir = dx < 0 ? -1 : 1;
    
    if (dy != 0) {
        const float m = static_cast<float>(dx) / static_cast<float>(dy);
        std::clog << "m: " << m << "\n";
        int x = ax;
        for (int i = 0; i <= dy; i++) {
            framebuffer.set(x, ay + i, color);
            //std::clog << "Colored " << x << ", " << ay + i << "\n";
            float px = m * (i + 1) + ax; // how much x has moved at y = i+1
            //std::clog << "At i: " << i << ", px: " << px << "\n";
            float d0 = px - x;
            float d1 = (x + dir) - px;
            if (std::abs(d0) - std::abs(d1) >= 0) {
                x += dir;
            }
        }
    }
}

void line(const int& ax, const int& ay, const int& bx, const int& by, TGAImage& framebuffer, const TGAColor& color) {
    const int dx = bx - ax;
    const int dy = by - ay;
    std::clog << "dx: " << dx << " & dy: " << dy << "\n";
    if (std::abs(dx) >= std::abs(dy)) {
        std::clog << "Drawing a shallow line\n";
        lineShallow(ax, ay, bx, by, framebuffer, color);
    } else {
        std::clog << "Drawing a steep line\n";
        lineSteep(ax, ay, bx, by, framebuffer, color);
    }
}

void log_openmp_status() {
#if RASTERIZER_PARALLEL
#ifdef _OPENMP
#pragma omp parallel
    {
#pragma omp single
        std::clog << "OpenMP enabled with " << omp_get_num_threads() << " threads\n";
    }
#else
    std::clog << "OpenMP rasterization requested, but OpenMP is NOT enabled\n";
#endif
#else
    std::clog << "OpenMP rasterization disabled\n";
#endif
}

int main(int argc, char** argv) {
    if (argc == 1) {
        return launch_gui();
    }

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/model.obj\n"
                  << "       " << argv[0] << " --demo\n"
                  << "       " << argv[0] << " --shader random|diffuse|phong|blinnphong --fill shaded|flat|intensity|barycentric|wireframe --output framebuffer.tga path/to/model.obj\n";
        return 1;
    }

    log_openmp_status();

    RenderSettings settings;
    std::string mode;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--fill") {
            if (i + 1 >= argc) {
                std::cerr << "--fill expects shaded, flat, intensity, barycentric, or wireframe\n";
                return 1;
            }
            const std::optional<ViewMode> parsed_view_mode = parse_view_mode(argv[++i]);
            if (!parsed_view_mode) {
                std::cerr << "Unknown fill mode: " << argv[i] << "\n";
                return 1;
            }
            settings.view_mode = *parsed_view_mode;
        } else if (arg == "--shader") {
            if (i + 1 >= argc) {
                std::cerr << "--shader expects random or diffuse\n";
                return 1;
            }
            const std::optional<ShaderMode> parsed_shader_mode = parse_shader_mode(argv[++i]);
            if (!parsed_shader_mode) {
                std::cerr << "Unknown shader: " << argv[i] << "\n";
                return 1;
            }
            settings.shader_mode = *parsed_shader_mode;
        } else if (arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "--output expects a .tga path\n";
                return 1;
            }
            settings.output_path = argv[++i];
        } else {
            mode = arg;
        }
    }
    if (mode.empty()) {
        std::cerr << "Missing model path or --demo\n";
        return 1;
    }

    RenderResult result;
    if (mode == "--demo") {
        result = render_demo(settings);
    } else {
        settings.model_paths = find_obj_files(mode);
        if (settings.model_paths.empty()) {
            std::cerr << "No OBJ files found at: " << mode << '\n';
            return 1;
        }

        result = render(settings);
    }

    if (!result.succeeded) {
        std::cerr << "Rendering failed with message: " << result.error << '\n';
        return 1;
    } else {
    std::clog << "Render time: " << result.duration_ms << " ms\n";
    }    
    /*
    TGAImage zbuffer_image = depthbuffer_to_image(zbuffer);
    zbuffer_image.write_tga_file("zbuffer.tga");
    write_depthbuffer_text(zbuffer, "zbuffer.txt");
    */
    return 0;
}
