#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "model.h"
#include "tgaimage.h"

#ifndef RASTERIZER_PARALLEL
#define RASTERIZER_PARALLEL 0
#endif

#if RASTERIZER_PARALLEL && defined(_OPENMP)
#include <omp.h>
#endif

constexpr int width  = 800;
constexpr int height = 800;

[[maybe_unused]] constexpr TGAColor white   = {{255, 255, 255, 255}, 4}; // attention, BGRA order
[[maybe_unused]] constexpr TGAColor green   = {{  0, 255,   0, 255}, 4};
[[maybe_unused]] constexpr TGAColor red     = {{  0,   0, 255, 255}, 4};
[[maybe_unused]] constexpr TGAColor blue    = {{255, 128,  64, 255}, 4};
[[maybe_unused]] constexpr TGAColor yellow  = {{  0, 200, 255, 255}, 4};

enum class FillMode {
    Flat,
    Intensity,
    Barycentric,
    Wireframe
};

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

std::tuple<int,int,double> project(vec3 v) {
    return { (v.x() + 1.) *  width/2,  // The input models are scaled to fit in [-1,1]^3, so shift x/y and scale them to screen space.
             (v.y() + 1.) * height/2,
             v.z() };
}

float triangle_area(const int& ax, const int& ay, const int& bx, const int& by, const int& cx, const int& cy) {
    // A = (ax(by - cy) + bx(cy - ay) + cx(ay - by)) / 2
    return 0.5 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
}

void triangle(const int& ax, const int& ay, const double& az, const int& bx, const int& by, const double& bz, const int& cx, const int& cy, const double& cz, TGAImage& framebuffer, FillMode fill_mode, TGAColor flat_color, std::vector<double>& depthbuffer) {
    // Bounding box
    // Look for minimum and max vertex coordinates
    const int min_x = std::min(std::min(ax, bx), cx);
    const int max_x = std::max(std::max(ax, bx), cx);
    const int min_y = std::min(std::min(ay, by), cy);
    const int max_y = std::max(std::max(ay, by), cy);

    const float total_area = triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area < 1) return;

#if RASTERIZER_PARALLEL && defined(_OPENMP)
#pragma omp parallel for
#endif
    for (int i = min_x; i <= max_x; i++) {
        for (int j = min_y; j <= max_y; j++) {
            // Calculate the barycentric coordinate
            // P = uA + vB + wC
            const double u = triangle_area(i, j, bx, by, cx, cy) / total_area;
            const double v = triangle_area(i, j, cx, cy, ax, ay) / total_area;
            const double w = triangle_area(i, j, ax, ay, bx, by) / total_area;

            if (u < 0 || v < 0 || w < 0) continue;

            const double z = u * az + v * bz + w * cz; // Compute depth
            const int depth_index = i + j * width;
            if (depthbuffer[depth_index] >= z) continue; // Larger z means closer to camera.

            depthbuffer[depth_index] = z;
            if (fill_mode == FillMode::Flat) {
                framebuffer.set(i, j, flat_color);
            } else if (fill_mode == FillMode::Intensity) {
                const double shade = std::clamp((z + 1.0) * 255.0 / 2.0, 0.0, 255.0);
                framebuffer.set(i, j, grayscale(static_cast<unsigned char>(shade)));
            } else if (fill_mode == FillMode::Barycentric) {
                framebuffer.set(i, j, barycentric_color(u, v, w));
            } else {
                framebuffer.set(i, j, wireframe(u, v, w));
            }
        }
    }
}

void render_model(TGAImage& framebuffer, std::vector<double>& depthbuffer, const std::string& model_path, FillMode fill_mode) {
    Model model(model_path);
    std::srand(1234);
    for (int i = 0; i < model.nfaces(); i++) { // Loop over the number of triangles need to be rendered
        auto [ax, ay, az] = project(model.vert(i, 0));
        auto [bx, by, bz] = project(model.vert(i, 1));
        auto [cx, cy, cz] = project(model.vert(i, 2));

        unsigned char c[3];
        for (int j=0; j<3; j++) c[j] = std::rand()%255;

        TGAColor flat_color = {{c[0], c[1], c[2], 255}, 4};
        
        triangle(ax, ay, az, bx, by, bz, cx, cy, cz,
                 framebuffer, fill_mode, flat_color, depthbuffer);
    }
}

void render_demo(TGAImage& framebuffer, std::vector<double>& depthbuffer, FillMode fill_mode) {
    int ax = 17, ay =  4, az =  13;
    int bx = 55, by = 39, bz = 128;
    int cx = 23, cy = 59, cz = 255;
    
    /*framebuffer.set(ax, ay, white);
    framebuffer.set(bx, by, white);
    framebuffer.set(cx, cy, white);
    
    line(ax, ay, bx, by, framebuffer, blue);
    line(cx, cy, bx, by, framebuffer, green);
    line(cx, cy, ax, ay, framebuffer, yellow);
    line(ax, ay, cx, cy, framebuffer, red);

    triangle(  7, 45, 35, 100, 45,  60, framebuffer, red);
    triangle(120, 35, 90,   5, 45, 110, framebuffer, white);
    triangle(115, 83, 80,  90, 85, 120, framebuffer, green);*/

    triangle(ax, ay, az, bx, by, bz, cx, cy, cz, framebuffer, fill_mode, red, depthbuffer);
}

void render_model_painters(TGAImage& framebuffer, std::vector<double>& depthbuffer, const std::string& model_path, FillMode fill_mode) {
    Model model(model_path);

    std::vector<int> face_order(model.nfaces());
    std::iota(face_order.begin(), face_order.end(), 0);
    std::sort(face_order.begin(), face_order.end(), [&](int a, int b) {
        auto face_z = [&](int iface) {
            vec3 v0 = model.vert(iface, 0);
            vec3 v1 = model.vert(iface, 1);
            vec3 v2 = model.vert(iface, 2);
            return (v0.z() + v1.z() + v2.z()) / 3.0;
        };

        return face_z(a) > face_z(b); // largest z first
    });

    std::srand(1234);
    for (int i : face_order) {
        auto [ax, ay, az] = project(model.vert(i, 0));
        auto [bx, by, bz] = project(model.vert(i, 1));
        auto [cx, cy, cz] = project(model.vert(i, 2));

        unsigned char c[3];
        for (int j=0; j<3; j++) c[j] = std::rand()%255;

        TGAColor flat_color = {{c[0], c[1], c[2], 255}, 4};
        
        triangle(ax, ay, az, bx, by, bz, cx, cy, cz,
                 framebuffer, fill_mode, flat_color, depthbuffer);
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
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/model.obj\n"
                  << "       " << argv[0] << " --demo\n"
                  << "       " << argv[0] << " --fill flat|intensity|barycentric path/to/model.obj\n";
        return 1;
    }

    log_openmp_status();

    TGAImage framebuffer(width, height, TGAImage::RGB);
    std::vector<double> depthbuffer(width * height, -std::numeric_limits<double>::infinity());

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
        render_demo(framebuffer, depthbuffer, fill_mode);
    } else {
        render_model(framebuffer, depthbuffer, mode, fill_mode);
    }
    const auto render_end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> render_duration = render_end - render_start;
    std::clog << "Render time: " << render_duration.count() << " ms\n";

    framebuffer.write_tga_file("framebuffer.tga");
    TGAImage zbuffer = depthbuffer_to_image(depthbuffer);
    zbuffer.write_tga_file("zbuffer.tga");
    return 0;
}
