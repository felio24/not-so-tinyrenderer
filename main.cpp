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
    const Model& model;
    TGAColor color = {};
    vec3 tri[3];  // triangle in eye coordinates

    RandomShader(const Model& m) : model(m) {}

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

struct SolidShader : IShader {
    TGAColor color;

    SolidShader(const TGAColor& c) : color(c) {}

    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const {
        return {false, color};                                    // do not discard the pixel
    }
};

struct DiffuseShader : IShader {
    const Model& model;
    TGAColor color = {};
    vec3 tri[3]; // triangle in eye/view/camera coordinates
    vec3 light;  // position of light source

    vec3 normal;
    double ambient_strength = 0.2;
    double diffuse_strength = 0.6;

    DiffuseShader(const Model& m, const double& ambient, const double& diffuse, const vec3& light) :
        model(m),
        light(light),
        ambient_strength(ambient),
        diffuse_strength(diffuse)
    {}

    // Coordinate space: eye/view space.
    // vertex(face, vert):
    //   - reads object-space model vertex
    //   - transforms it by ModelView into eye-space
    //   - stores eye-space vertex position in tri[vert]
    //   - returns projected clip-space position
    //
    // fragment(bar):
    //   - receives barycentric weights only
    //   - reconstructs eye-space fragment position p from tri[] and bar
    //   - computes light vector L = normalize(light_eye - p)
    //   - computes view vector V = normalize(-p), because camera is origin in eye space
    //   - uses face normal N already computed in eye space

    virtual vec4 vertex(const int face, const int vert) {
        vec3 v = model.vert(face, vert);                               // current vertex in object coordinates
        vec4 gl_Position = ModelView * vec4{v.x(), v.y(), v.z(), 1.};

        tri[vert] = gl_Position.xyz();                                 // in eye/view/camera coordinates
        if (vert == 2) {
            normal = unit_vector(cross(tri[1]-tri[0], tri[2]-tri[0])); // direction depends on vertex winding and cross-product order
        }

        return Perspective * gl_Position;                              // in clip coordinates
    }

    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const {
        const vec3 p = tri[0] * bar.x() + tri[1] * bar.y() + tri[2] * bar.z(); // bar is uvw which are the barycentric coordinates, not {x, y, z} in model/world/eye space
        const vec3 l = unit_vector(light-p);                             // so l differs for each point in the face?
        const double diffuse = std::max(0., dot(normal,l));

        // weighted sum
        double total = ambient_strength + diffuse_strength * diffuse;
        total = std::clamp(total, 0.0, 1.0);
        uint8_t intensity = static_cast<uint8_t>(total * 255);
        return {false, {{intensity, intensity, intensity, 255}, 4}};     // do not discard the pixel
    }
};

struct PhongSpecularShader : DiffuseShader {
    double specular_strength = 0.2;
    double shininess = 16.0;

    PhongSpecularShader(
        const Model& m,
        const double& ambient,
        const double& diffuse,
        const vec3& light,
        const double& specular,
        const double& exponent
    ) :
        DiffuseShader(m, ambient, diffuse, light),
        specular_strength(specular),
        shininess(exponent)
    {}

    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const override {
        // TODO: reflection-vector specular:
        // p = interpolated eye-space fragment position
        // L = normalize(light - p)
        // V = normalize(eye-p)
        // R = normalize(2 * dot(normal, L) * normal - L)
        // specular = pow(max(0, dot(R, V)), shininess)
        const vec3 p = tri[0] * bar.x() + tri[1] * bar.y() + tri[2] * bar.z();
        const vec3 l = unit_vector(light-p);
        const double n_dot_l = dot(normal, l);
        const double diffuse = std::max(0., n_dot_l);

        double specular = 0.0;
        if (n_dot_l > 0.0) { // ensure that the light is actually in front of the face
            const vec3 r = unit_vector(2 * n_dot_l * normal - l);
            const vec3 v = unit_vector(-p); // because this calculation is done in the eye-space where the eye/camera is at the origin {0,0,0}
            specular = std::pow(std::max(0., dot(r,v)), shininess);
        }
        
        // weighted sum
        TGAColor gl_FragColor = {{255, 255, 255, 255}, 4};
        double total = ambient_strength + diffuse_strength * diffuse + specular_strength * specular;
        total = std::clamp(total, 0.0, 1.0);
        //uint8_t intensity = static_cast<uint8_t>(total * 255);
        for (int channel : {0,1,2})
            gl_FragColor[channel] *= std::min(1., total);
        return {false, gl_FragColor};     // do not discard the pixel
    }
};

struct BlinnPhongSpecularShader : DiffuseShader {
    double specular_strength = 0.2;
    double shininess = 16.0;

    BlinnPhongSpecularShader(
        const Model& m,
        const double& ambient,
        const double& diffuse,
        const vec3& light,
        const double& specular,
        const double& exponent
    ) :
        DiffuseShader(m, ambient, diffuse, light),
        specular_strength(specular),
        shininess(exponent)
    {}

    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const override {
        // TODO: half-vector specular:
        // p = interpolated eye-space fragment position
        // L = normalize(light - p)
        // V = normalize(-p)
        // H = normalize(L + V)
        // specular = pow(max(0, dot(normal, H)), shininess)
        const vec3 p = tri[0] * bar.x() + tri[1] * bar.y() + tri[2] * bar.z();
        const vec3 l = unit_vector(light-p);
        const double n_dot_l = dot(normal, l);
        const double diffuse = std::max(0., n_dot_l);

        double specular = 0.0;
        if (n_dot_l > 0.0) {
            const vec3 v = unit_vector(-p);
            const vec3 h = unit_vector(l + v);
            specular = std::pow(std::max(0., dot(normal,h)), shininess);
        }

        // weighted sum
        TGAColor gl_FragColor = {{255, 255, 255, 255}, 4};
        double total = ambient_strength + diffuse_strength * diffuse + specular_strength * specular;
        total = std::clamp(total, 0.0, 1.0);
        //uint8_t intensity = static_cast<uint8_t>(total * 255);
        for (int channel : {0,1,2})
            gl_FragColor[channel] *= std::min(1., total);
        return {false, gl_FragColor};     // do not discard the pixel
    }
};

enum class ShaderMode {
    Random,
    Diffuse,
    PhongSpecular,
    BlinnPhongSpecular
};

[[maybe_unused]] constexpr TGAColor white   = {{255, 255, 255, 255}, 4}; // attention, BGRA order
[[maybe_unused]] constexpr TGAColor green   = {{  0, 255,   0, 255}, 4};
[[maybe_unused]] constexpr TGAColor red     = {{  0,   0, 255, 255}, 4};
[[maybe_unused]] constexpr TGAColor blue    = {{255, 128,  64, 255}, 4};
[[maybe_unused]] constexpr TGAColor yellow  = {{  0, 200, 255, 255}, 4};

// FillMode enum moved to notverygl.h

std::optional<FillMode> parse_fill_mode(const std::string& mode) {
    if (mode == "shaded") return FillMode::Shaded;
    if (mode == "flat") return FillMode::Flat;
    if (mode == "intensity") return FillMode::Intensity;
    if (mode == "barycentric") return FillMode::Barycentric;
    if (mode == "wireframe") return FillMode::Wireframe;
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

template<typename Shader>
void render_model_with_shader(TGAImage& framebuffer, const Model& model, Shader& shader, FillMode fill_mode) {
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
        shader.color = flat_color;
        
        rasterize(clip, shader, framebuffer, fill_mode, flat_color);
    }
}

void render_model(TGAImage& framebuffer, const std::string& model_path, FillMode fill_mode, ShaderMode shader_mode) {
    const vec3 light_world{2,2,3};
    const vec4 light_eye = ModelView * vec4{light_world.x(), light_world.y(), light_world.z(), 1.};
    Model model(model_path);

    if (shader_mode == ShaderMode::Random) {
        RandomShader shader(model);
        render_model_with_shader(framebuffer, model, shader, fill_mode);
    } else if (shader_mode == ShaderMode::PhongSpecular) {
        PhongSpecularShader shader(model, 0.1, 0.7, light_eye.xyz(), 0.5, 32.0);
        render_model_with_shader(framebuffer, model, shader, fill_mode);
    } else if (shader_mode == ShaderMode::BlinnPhongSpecular) {
        BlinnPhongSpecularShader shader(model, 0.1, 0.7, light_eye.xyz(), 0.5, 32.0);
        render_model_with_shader(framebuffer, model, shader, fill_mode);
    } else {
        DiffuseShader shader(model, 0.1, 0.7, light_eye.xyz());
        render_model_with_shader(framebuffer, model, shader, fill_mode);
    }
}

void render_demo(TGAImage& framebuffer, FillMode fill_mode) {
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

    SolidShader far_shader(blue);
    SolidShader near_shader(red);
    rasterize(far, far_shader, framebuffer, fill_mode, blue);
    rasterize(near, near_shader, framebuffer, fill_mode, red);
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
                  << "       " << argv[0] << " --shader random|diffuse|phong|blinnphong --fill shaded|flat|intensity|barycentric|wireframe --output framebuffer.tga path/to/model.obj\n";
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

    FillMode fill_mode = FillMode::Shaded;
    ShaderMode shader_mode = ShaderMode::Diffuse;
    std::string output_path = "framebuffer.tga";
    std::string mode;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--fill") {
            if (i + 1 >= argc) {
                std::cerr << "--fill expects shaded, flat, intensity, barycentric, or wireframe\n";
                return 1;
            }
            const std::optional<FillMode> parsed_fill_mode = parse_fill_mode(argv[++i]);
            if (!parsed_fill_mode) {
                std::cerr << "Unknown fill mode: " << argv[i] << "\n";
                return 1;
            }
            fill_mode = *parsed_fill_mode;
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
            shader_mode = *parsed_shader_mode;
        } else if (arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "--output expects a .tga path\n";
                return 1;
            }
            output_path = argv[++i];
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
        render_model(framebuffer, mode, fill_mode, shader_mode);
    }
    const auto render_end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> render_duration = render_end - render_start;
    std::clog << "Render time: " << render_duration.count() << " ms\n";

    framebuffer.write_tga_file(output_path.c_str());
    TGAImage zbuffer_image = depthbuffer_to_image(zbuffer);
    zbuffer_image.write_tga_file("zbuffer.tga");
    write_depthbuffer_text(zbuffer, "zbuffer.txt");
    return 0;
}
