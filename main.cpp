#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
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

constexpr TGAColor white   = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

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

std::pair<int,int> project(vec3 v) { // First of all, (x,y) is an orthogonal projection of the vector (x,y,z), so drop the z completely
    return { (v.x() + 1.) * width/2,   // Second, since the input models are scaled to have fit in the [-1,1]^3 world coordinates,
             (v.y() + 1.) * height/2 }; // we want to shift the vector (x,y) and then scale it to span the entire screen.
}

float triangle_area(const int& ax, const int& ay, const int& bx, const int& by, const int& cx, const int& cy) {
    // A = (ax(by - cy) + bx(cy - ay) + cx(ay - by)) / 2
    return 0.5 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
}

void triangle(const int& ax, const int& ay, const int& az, const int& bx, const int& by, const int& bz, const int& cx, const int& cy, const int& cz, TGAImage &framebuffer) {
    /* Attempt at scanline
    // Bubble sort in ascending y
    if (ay > by) {
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    if (by > cy) {
        std::swap(bx, cx);
        std::swap(by, cy);
    }
    if (ay > by) {
        std::swap(ax, bx);
        std::swap(ay, by);
    }

    // Split into top half and bottom half at b
    // Find the x coordinate between ac

    line(ax, ay, bx, by, framebuffer, color);
    line(bx, by, cx, cy, framebuffer, color);
    line(cx, cy, ax, ay, framebuffer, color);
    */

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
            unsigned char z = static_cast<unsigned char>(u * az + v * bz + w * cz);
            framebuffer.set(i, j, {z,0,z,0}); // BGRA, alpha doesn't matter for now
        }
    }
}

void render_model(TGAImage& framebuffer, const std::string& model_path) {
    Model model(model_path);
    std::srand(1234);
    for (int i = 0; i < model.nfaces(); i++) { // Loop over the number of triangles need to be rendered
        vec3 v0 = model.vert(i, 0);
        std::pair<int,int> p0 = project(v0);
        vec3 v1 = model.vert(i, 1);
        std::pair<int,int> p1 = project(v1);
        vec3 v2 = model.vert(i, 2);
        std::pair<int,int> p2 = project(v2);

        int c[3];
        for (int j=0; j<3; j++) c[j] = std::rand()%255;
        
        triangle(p0.first, p0.second, c[0], p1.first, p1.second, c[1], p2.first, p2.second, c[2], framebuffer);
    }
}

void render_demo(TGAImage& framebuffer) {
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

    triangle(ax, ay, az, bx, by, bz, cx, cy, cz, framebuffer);
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
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/model.obj\n"
                  << "       " << argv[0] << " --demo\n";
        return 1;
    }

    log_openmp_status();

    TGAImage framebuffer(width, height, TGAImage::RGB);

    const std::string mode = argv[1];
    const auto render_start = std::chrono::steady_clock::now();
    if (mode == "--demo") {
        render_demo(framebuffer);
    } else {
        render_model(framebuffer, mode);
    }
    const auto render_end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> render_duration = render_end - render_start;
    std::clog << "Render time: " << render_duration.count() << " ms\n";

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
