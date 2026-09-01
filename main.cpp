#include <algorithm>
#include <chrono>
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
#include "mat.h"
#include "model.h"
#include "notverygl.h"
#include "tgaimage.h"

#ifndef RASTERIZER_PARALLEL
#define RASTERIZER_PARALLEL 0
#endif

#if RASTERIZER_PARALLEL && defined(_OPENMP)
#include <omp.h>
#endif

constexpr int width  = 800;
constexpr int height = 800;

extern mat4 ModelView, Perspective; // "OpenGL" state matrices and
extern std::vector<double> zbuffer; // the depth buffer

struct RandomShader : IShader {
    const Model &model;
    TGAColor color = {};
    vec3 tri[3];  // triangle in eye coordinates

    RandomShader(const Model &m) : model(m) {
    }

    virtual vec4 vertex(const int face, const int vert) {
        vec3 v = model.vert(face, vert);                          // current vertex in object coordinates
        vec4 gl_Position = ModelView * vec4{v.x(), v.y(), v.z(), 1.};
        tri[vert] = gl_Position.xyz();                            // in eye coordinates
        return Perspective * gl_Position;                         // in clip coordinates
    }

    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const {
        return {false, color};                                    // do not discard the pixel
    }
};

struct DemoShader : IShader {
    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const {
        return {false, {{255, 255, 255, 255}, 4}};                // do not discard the pixel
    }
};

[[maybe_unused]] constexpr TGAColor white   = {{255, 255, 255, 255}, 4}; // attention, BGRA order
[[maybe_unused]] constexpr TGAColor green   = {{  0, 255,   0, 255}, 4};
[[maybe_unused]] constexpr TGAColor red     = {{  0,   0, 255, 255}, 4};
[[maybe_unused]] constexpr TGAColor blue    = {{255, 128,  64, 255}, 4};
[[maybe_unused]] constexpr TGAColor yellow  = {{  0, 200, 255, 255}, 4};

// FillMode enum moved to notverygl.h

std::optional<FillMode> parse_fill_mode(const std::string& mode) {
    if (mode == "flat") return FillMode::Flat;
    if (mode == "intensity") return FillMode::Intensity;
    if (mode == "barycentric") return FillMode::Barycentric;
    if (mode == "wireframe") return FillMode::Wireframe;
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

void render_model(TGAImage& framebuffer, const std::string& model_path, FillMode fill_mode) {
    Model model(model_path);
    RandomShader shader(model);
    std::srand(1234);
    for (int i = 0; i < model.nfaces(); i++) { // Loop over the number of triangles need to be rendered
        vec4 clip[3];
        for (int d : {0,1,2}) {  
            // assemble the primitive
            clip[d] = shader.vertex(i, d);
        }

        unsigned char c[3];
        for (int j=0; j<3; j++) c[j] = std::rand()%255;

        TGAColor flat_color = {{c[0], c[1], c[2], 255}, 4};
        
        rasterize(clip, shader, framebuffer, fill_mode, flat_color);
    }
}

void render_demo(TGAImage& framebuffer, FillMode fill_mode) {
    DemoShader shader;

    Triangle far = {
        vec4{-0.63, -0.69, -0.6, 1.0},
        vec4{ 0.71, -0.51, -0.6, 1.0},
        vec4{-0.29,  0.80, -0.6, 1.0},
    };
    Triangle near = {
        vec4{-0.43, -0.40, 0.6, 1.0},
        vec4{ 0.91, -0.20, 0.6, 1.0},
        vec4{ 0.09,  0.97, 0.6, 1.0},
    };

    rasterize(far, shader, framebuffer, fill_mode, blue);
    rasterize(near, shader, framebuffer, fill_mode, red);
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
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/model.obj\n"
                  << "       " << argv[0] << " --demo\n"
                  << "       " << argv[0] << " --fill flat|intensity|barycentric path/to/model.obj\n";
        return 1;
    }

    log_openmp_status();

    const vec3    eye{-1,0,2}; // camera position
    const vec3 center{0,0,0};  // camera direction
    const vec3     up{0,1,0};  // camera up vector

    lookat(eye, center, up);                                   // build the ModelView   matrix
    init_perspective((eye-center).length());                   // build the Perspective matrix
    init_viewport(width/16, height/16, width*7/8, height*7/8); // build the Viewport    matrix
    init_zbuffer(width, height);

    TGAImage framebuffer(width, height, TGAImage::RGB);

    FillMode fill_mode = FillMode::Flat;
    std::string mode;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--fill") {
            if (i + 1 >= argc) {
                std::cerr << "--fill expects flat, intensity, barycentric, or wireframe\n";
                return 1;
            }
            const std::optional<FillMode> parsed_fill_mode = parse_fill_mode(argv[++i]);
            if (!parsed_fill_mode) {
                std::cerr << "Unknown fill mode: " << argv[i] << "\n";
                return 1;
            }
            fill_mode = *parsed_fill_mode;
        } else {
            mode = arg;
        }
    }
    if (mode.empty()) {
        std::cerr << "Missing model path or --demo\n";
        return 1;
    }

    const auto render_start = std::chrono::steady_clock::now();
    if (mode == "--demo") {
        render_demo(framebuffer, fill_mode);
    } else {
        render_model(framebuffer, mode, fill_mode);
    }
    const auto render_end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> render_duration = render_end - render_start;
    std::clog << "Render time: " << render_duration.count() << " ms\n";

    framebuffer.write_tga_file("framebuffer.tga");
    TGAImage zbuffer_image = depthbuffer_to_image(zbuffer);
    zbuffer_image.write_tga_file("zbuffer.tga");
    write_depthbuffer_text(zbuffer, "zbuffer.txt");
    return 0;
}
