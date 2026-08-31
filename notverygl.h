#include "tgaimage.h"
#include "vec3.h"
#include "mat.h"
// Fill modes for triangle rasterization
enum class FillMode {
    Flat,
    Intensity,
    Barycentric,
    Wireframe
};

void lookat(const vec3& eye, const vec3& center, const vec3& up);
void init_perspective(const double& f);
void init_viewport(const int& x, const int& y, const int& w, const int& h);
void init_zbuffer(const int& width, const int& height);

struct IShader {
    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const = 0;
};

typedef vec4 Triangle[3]; // a triangle primitive is made of three ordered points
// Helper color functions defined in main.cpp
TGAColor grayscale(unsigned char v);
TGAColor barycentric_color(double u, double v, double w);
TGAColor wireframe(double u, double v, double w);

// Rasterize a triangle (fill_mode and flat_color are provided by caller)
void rasterize(const Triangle &clip, const IShader &shader, TGAImage &framebuffer, FillMode fill_mode, const TGAColor &flat_color);