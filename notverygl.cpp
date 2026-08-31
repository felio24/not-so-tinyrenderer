#include <algorithm>
#include "notverygl.h"

mat4 ModelView, Viewport, Perspective;
std::vector<double> zbuffer;

void lookat(const vec3& eye, const vec3& center, const vec3& up) {
    vec3 n = unit_vector(eye - center);
    vec3 l = unit_vector(cross(up, n));
    vec3 m = unit_vector(cross(n, l));
    ModelView = mat4{{{l.x(),l.y(),l.z(),0}, {m.x(),m.y(),m.z(),0}, {n.x(),n.y(),n.z(),0}, {0,0,0,1}}} *
                mat4{{{1,0,0,-center.x()}, {0,1,0,-center.y()}, {0,0,1,-center.z()}, {0,0,0,1}}};
}

void init_perspective(const double& f) {
    Perspective = {{{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0, -1/f,1}}};
}

void init_viewport(const int& x, const int& y, const int& w, const int& h) {
    Viewport = {{{w/2., 0, 0, x+w/2.}, {0, h/2., 0, y+h/2.}, {0,0,1,0}, {0,0,0,1}}};
}

void init_zbuffer(const int& width, const int& height) {
    zbuffer = std::vector(width*height, -1000.);
}

float triangle_area(const int& ax, const int& ay, const int& bx, const int& by, const int& cx, const int& cy) {
    /*
    const int ax = static_cast<int>(screen[0].x());
    const int ay = static_cast<int>(screen[0].y());
    const int bx = static_cast<int>(screen[1].x());
    const int by = static_cast<int>(screen[1].y());
    const int cx = static_cast<int>(screen[2].x());
    const int cy = static_cast<int>(screen[2].y());
    */
    
    // A = (ax(by - cy) + bx(cy - ay) + cx(ay - by)) / 2
    return 0.5 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
}

void rasterize(const Triangle &clip, const IShader &shader, TGAImage &framebuffer, FillMode fill_mode, const TGAColor &flat_color) {
    vec4 ndc[3] = {
        clip[0] / clip[0].w(),
        clip[1] / clip[1].w(),
        clip[2] / clip[2].w()
    };
    vec4 screen[3] = {
        Viewport * ndc[0],
        Viewport * ndc[1],
        Viewport * ndc[2]
    };

    mat3 ABC = {{ {screen[0].x(), screen[0].y(), 1.}, 
                  {screen[1].x(), screen[1].y(), 1.}, 
                  {screen[2].x(), screen[2].y(), 1.} }};

    const float total_area = triangle_area(screen[0].x(),screen[0].y(), screen[1].x(), screen[1].y(), screen[2].x(), screen[2].y());
    if (total_area < 1) return;

    // Bounding box
    // Look for minimum and max vertex coordinates
    auto [bbminx,bbmaxx] = std::minmax({screen[0].x(), screen[1].x(), screen[2].x()}); // bounding box for the triangle
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y(), screen[1].y(), screen[2].y()}); // defined by its top left and bottom right corners

#if RASTERIZER_PARALLEL && defined(_OPENMP)
#pragma omp parallel for
#endif
    for (int i = std::max<int>(bbminx, 0); i <= std::min<int>(bbmaxx, framebuffer.width()-1); i++) {
        for (int j = std::max<int>(bbminy, 0); j <= std::min<int>(bbmaxy, framebuffer.height()-1); j++) {
            // Calculate the barycentric coordinate
            // P = uA + vB + wC
            const double u = triangle_area(i, j, screen[1].x(), screen[1].y(), screen[2].x(), screen[2].y()) / total_area;
            const double v = triangle_area(i, j, screen[2].x(), screen[2].y(), screen[0].x(), screen[0].y()) / total_area;
            const double w = triangle_area(i, j, screen[0].x(), screen[0].y(), screen[1].x(), screen[1].y()) / total_area;

            if (u < 0 || v < 0 || w < 0) continue;

            const double z = u * screen[0].z() + v * screen[1].z() + w * screen[2].z(); // Compute depth
            const int depth_index = i + j * framebuffer.width();
            if (zbuffer[depth_index] >= z) continue; // Larger z means closer to camera.
            
            zbuffer[depth_index] = z;
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