#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include "model.h"
#include "tgaimage.h"

constexpr int width  = 280;
constexpr int height = 280;

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

void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage &framebuffer, TGAColor color) {
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
    std::clog << "Total triangle area is " << total_area << std::endl;

    for (int i = min_x; i <= max_x; i++) {
        for (int j = min_y; j <= max_y; j++) {
            // Calculate the barycentric coordinate
            // P = uA + vB + wC
            const double u = triangle_area(i, j, bx, by, cx, cy) / total_area;
            const double v = triangle_area(i, j, cx, cy, ax, ay) / total_area;
            const double w = triangle_area(i, j, ax, ay, bx, by) / total_area;

            if (u < 0 || v < 0 || w < 0) continue;
            framebuffer.set(i, j, color);
        }
    }
}

void render_model(TGAImage& framebuffer, const std::string& model_path) {
    Model model(model_path);

    for (int i = 0; i < model.nfaces(); i++) { // Loop over the number of triangles need to be rendered
        vec3 v0 = model.vert(i, 0);
        std::pair<int,int> p0 = project(v0);
        vec3 v1 = model.vert(i, 1);
        std::pair<int,int> p1 = project(v1);
        vec3 v2 = model.vert(i, 2);
        std::pair<int,int> p2 = project(v2);

        line(p0.first, p0.second, p1.first, p1.second, framebuffer, red);
        line(p0.first, p0.second, p2.first, p2.second, framebuffer, red);
        line(p1.first, p1.second, p2.first, p2.second, framebuffer, red);
    }
}

void render_demo(TGAImage& framebuffer) {
    /*int ax =  7, ay =  3;
    int bx = 12, by = 37;
    int cx = 62, cy = 30;
    
    framebuffer.set(ax, ay, white);
    framebuffer.set(bx, by, white);
    framebuffer.set(cx, cy, white);
    
    line(ax, ay, bx, by, framebuffer, blue);
    line(cx, cy, bx, by, framebuffer, green);
    line(cx, cy, ax, ay, framebuffer, yellow);
    line(ax, ay, cx, cy, framebuffer, red);*/

    triangle(  7, 45, 35, 100, 45,  60, framebuffer, red);
    triangle(120, 35, 90,   5, 45, 110, framebuffer, white);
    triangle(115, 83, 80,  90, 85, 120, framebuffer, green);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/model.obj\n"
                  << "       " << argv[0] << " --demo\n";
        return 1;
    }

    TGAImage framebuffer(width, height, TGAImage::RGB);

    const std::string mode = argv[1];
    if (mode == "--demo") {
        render_demo(framebuffer);
    } else {
        render_model(framebuffer, mode);
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
