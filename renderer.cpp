#include <chrono>
#include "model.h"
#include "renderer.h"

extern mat4 ModelView, Perspective; // "OpenGL" state matrices

[[maybe_unused]] constexpr TGAColor white   = {{255, 255, 255, 255}, 4}; // attention, BGRA order
[[maybe_unused]] constexpr TGAColor green   = {{  0, 255,   0, 255}, 4};
[[maybe_unused]] constexpr TGAColor red     = {{  0,   0, 255, 255}, 4};
[[maybe_unused]] constexpr TGAColor blue    = {{255, 128,  64, 255}, 4};
[[maybe_unused]] constexpr TGAColor yellow  = {{  0, 200, 255, 255}, 4};

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
    vec3 tri[3];      // triangle in eye/view/camera coordinates
    vec3 tri_norm[3]; // normal of triangle vertex in eye/view/camera coordinates
    vec2 tri_uv[3];   // uv coordinate of triangle vertex
    vec3 light;       // position of light source
    vec3 eye;         // position of eye in eye/view/camera coordinates

    mat<3,2> TB;
    double ambient_strength = 0.2;
    double diffuse_strength = 0.6;

    DiffuseShader(const Model& m, const double& ambient, const double& diffuse, const vec3& light, const vec3& world_eye) :
        model(m),
        light(light),
        ambient_strength(ambient),
        diffuse_strength(diffuse)
    {
        const vec4 eye_eye = ModelView * vec4{world_eye.x(), world_eye.y(), world_eye.z(), 1};
        eye = eye_eye.xyz();
    }

    vec3 interpolateNormal(const vec3 bar) const {
        return unit_vector(
            tri_norm[0] * bar.x() +
            tri_norm[1] * bar.y() +
            tri_norm[2] * bar.z()
        );
    }

    vec2 interpolateUV(const vec3 bar) const {
        return (tri_uv[0] * bar.x() +
            tri_uv[1] * bar.y() +
            tri_uv[2] * bar.z()
        );
    }

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
    //   - computes light vector L = normalize(light_eye - p) for point light, normalize(light) for directional light
    //   - computes view vector V = normalize(-p), because camera is origin in eye space
    //   - uses interpolated normal retrieved from gl_norm_Position already computed in eye space

    virtual vec4 vertex(const int face, const int vert) {
        vec3 v = model.vert(face, vert);                               // current vertex in object coordinates
        vec4 gl_Position = ModelView * vec4{v.x(), v.y(), v.z(), 1.};

        vec3 object_normal = model.norm(face, vert);
        vec4 eye_normal = ModelView * vec4{object_normal.x(), object_normal.y(), object_normal.z(), 0.};

        tri[vert] = gl_Position.xyz();                                 // in eye/view/camera coordinates
        tri_norm[vert] = eye_normal.xyz();
        if (model.has_normalmap() || model.has_diffusemap() || model.has_specularmap()) {
            tri_uv[vert] = model.uv(face, vert);   
        }
        if (vert == 2) {
            if (model.normalmap_space() == NormalMapSpace::Tangent) {
                // TBN matrix setup
                const vec3 e0 = tri[1] - tri[0];
                const vec3 e1 = tri[2] - tri[0];
                const vec2 u0 = tri_uv[1] - tri_uv[0];
                const vec2 u1 = tri_uv[2] - tri_uv[0];

                const mat<3,2> E{{e0.x(),e1.x()},{e0.y(),e1.y()},{e0.z(),e1.z()}};
                const mat2 U{{u0.x(), u1.x()}, {u0.y(), u1.y()}};
                TB = E * U.inverse();
                
                const vec3 unit_T = unit_vector(vec3(TB[0][0], TB[1][0], TB[2][0]));
                const vec3 unit_B = unit_vector(vec3(TB[0][1], TB[1][1], TB[2][1]));
                TB[0][0] = unit_T[0]; TB[1][0] = unit_T[1]; TB[2][0] = unit_T[2];
                TB[0][1] = unit_B[0]; TB[1][1] = unit_B[1]; TB[2][1] = unit_B[2];
            }
        }

        return Perspective * gl_Position;                              // in clip coordinates
    }

    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const {
        // const vec3 p = tri[0] * bar.x() + tri[1] * bar.y() + tri[2] * bar.z(); // bar is uvw which are the barycentric coordinates, not {x, y, z} in model/world/eye space
        // const vec3 point_l = unit_vector(light-p);
        const vec2 interpolated_uv = interpolateUV(bar);
        const vec3 directional_l = unit_vector(light); // directional l is constant for all fragments
        const double diffuse = std::max(0., dot(interpolateNormal(bar), directional_l));

        // weighted sum
        TGAColor gl_FragColor = {{255, 255, 255, 255}, 4};
        if (model.has_diffusemap()) {
            gl_FragColor = model.diffusemap_sample(interpolated_uv);
        }
        double total = ambient_strength + diffuse_strength * diffuse;
        for (int channel : {0,1,2}) {
            gl_FragColor[channel] = static_cast<std::uint8_t>(std::min(255.0, gl_FragColor[channel] * total));
        }
        return {false, gl_FragColor};     // do not discard the pixel
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
        const double& exponent,
        const vec3& world_eye
    ) :
        DiffuseShader(m, ambient, diffuse, light, world_eye),
        specular_strength(specular),
        shininess(exponent)
    {}

    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const override {
        vec3 interpolated_n = interpolateNormal(bar);
        const vec2 interpolated_uv = interpolateUV(bar);
        if (model.has_normalmap()) {
            TGAColor sample = model.normalmap_sample(interpolated_uv);
            const vec3 sampled_normal{
                sample[2] / 255.0 * 2.0 - 1.0, // red -> x
                sample[1] / 255.0 * 2.0 - 1.0, // green -> y
                sample[0] / 255.0 * 2.0 - 1.0  // blue -> z
            };

            if (model.normalmap_space() == NormalMapSpace::Tangent) {
                const mat3 TBN{{
                    {TB[0][0], TB[0][1], interpolated_n.x()},
                    {TB[1][0], TB[1][1], interpolated_n.y()},
                    {TB[2][0], TB[2][1], interpolated_n.z()}
                }};

                interpolated_n = unit_vector(TBN * sampled_normal);
            } else {
                interpolated_n = unit_vector(
                    (ModelView * vec4{
                        sampled_normal.x(),
                        sampled_normal.y(),
                        sampled_normal.z(),
                        0.0
                    }).xyz()
                );
            }
        }
        // TODO: reflection-vector specular:
        // p = interpolated eye-space fragment position
        // L = normalize(light - p)
        // V = normalize(eye-p)
        // R = normalize(2 * dot(normal, L) * normal - L)
        // specular = pow(max(0, dot(R, V)), shininess)
        const vec3 p = tri[0] * bar.x() + tri[1] * bar.y() + tri[2] * bar.z();
        // const vec3 point_l = unit_vector(light-p);
        const vec3 directional_l = unit_vector(light);
        const double n_dot_l = dot(interpolated_n, directional_l);
        const double diffuse = std::max(0., n_dot_l);

        double specular = 0.0;
        if (n_dot_l > 0.0) { // ensure that the light is actually in front of the face
            const vec3 r = unit_vector(2 * n_dot_l * interpolated_n - directional_l);
            const vec3 v = unit_vector(eye-p);
            double specular_mask = 1.0;
            if (model.has_specularmap()) {
                specular_mask = static_cast<double>(model.specularmap_sample(interpolated_uv)[0]) / 255.0;
            }
            specular = 3 * specular_mask * std::pow(std::max(0., dot(r,v)), shininess);
        }
        
        // weighted sum
        TGAColor gl_FragColor = {{255, 255, 255, 255}, 4};
        if (model.has_diffusemap()) {
            gl_FragColor = model.diffusemap_sample(interpolated_uv);
        }
        double total = ambient_strength + diffuse_strength * diffuse + specular_strength * specular;
        for (int channel : {0,1,2}) {
            gl_FragColor[channel] = static_cast<std::uint8_t>(std::min(255.0, gl_FragColor[channel] * total));
        }
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
        const double& exponent,
        const vec3& world_eye
    ) :
        DiffuseShader(m, ambient, diffuse, light, world_eye),
        specular_strength(specular),
        shininess(exponent)
    {}

    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const override {
        vec3 interpolated_n = interpolateNormal(bar);
        const vec2 interpolated_uv = interpolateUV(bar);
        if (model.has_normalmap()) {
            TGAColor sample = model.normalmap_sample(interpolated_uv);
            const vec3 sampled_normal{
                sample[2] / 255.0 * 2.0 - 1.0, // red -> x
                sample[1] / 255.0 * 2.0 - 1.0, // green -> y
                sample[0] / 255.0 * 2.0 - 1.0  // blue -> z
            };

            if (model.normalmap_space() == NormalMapSpace::Tangent) {
                const mat3 TBN{{
                    {TB[0][0], TB[0][1], interpolated_n.x()},
                    {TB[1][0], TB[1][1], interpolated_n.y()},
                    {TB[2][0], TB[2][1], interpolated_n.z()}
                }};

                interpolated_n = unit_vector(TBN * sampled_normal);
            } else {
                interpolated_n = unit_vector(
                    (ModelView * vec4{
                        sampled_normal.x(),
                        sampled_normal.y(),
                        sampled_normal.z(),
                        0.0
                    }).xyz()
                );
            }
        }
        // TODO: half-vector specular:
        // p = interpolated eye-space fragment position
        // L = normalize(light - p)
        // V = normalize(-p)
        // H = normalize(L + V)
        // specular = pow(max(0, dot(normal, H)), shininess)
        const vec3 p = tri[0] * bar.x() + tri[1] * bar.y() + tri[2] * bar.z();
        // const vec3 point_l = unit_vector(light-p);
        const vec3 directional_l = unit_vector(light);
        const double n_dot_l = dot(interpolated_n, directional_l);
        const double diffuse = std::max(0., n_dot_l);

        double specular = 0.0;
        if (n_dot_l > 0.0) {
            const vec3 v = unit_vector(eye-p);
            const vec3 h = unit_vector(directional_l + v);
            double specular_mask = 1.0;
            if (model.has_specularmap()) {
                specular_mask = static_cast<double>(model.specularmap_sample(interpolated_uv)[0]) / 255.0;
            }
            specular = 3 * specular_mask * std::pow(std::max(0., dot(interpolated_n,h)), shininess);
        }

        // weighted sum
        TGAColor gl_FragColor = {{255, 255, 255, 255}, 4};
        if (model.has_diffusemap()) {
            gl_FragColor = model.diffusemap_sample(interpolated_uv);
        }
        double total = ambient_strength + diffuse_strength * diffuse + specular_strength * specular;
        for (int channel : {0,1,2}) {
            gl_FragColor[channel] = static_cast<std::uint8_t>(std::min(255.0, gl_FragColor[channel] * total));
        }
        return {false, gl_FragColor};     // do not discard the pixel
    }
};

std::vector<std::filesystem::path> find_obj_files(
    const std::filesystem::path& input
) {
    namespace fs = std::filesystem;
    std::vector<fs::path> models;

    if (fs::is_regular_file(input) && input.extension() == ".obj") {
        models.push_back(input);
    } else if (fs::is_directory(input)) {
        for (const fs::directory_entry& entry :
             fs::recursive_directory_iterator(input)) {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".obj") {
                models.push_back(entry.path());
            }
        }
    }

    std::sort(models.begin(), models.end());
    return models;
}

template<typename Shader>
void render_model_with_shader(TGAImage& framebuffer, const Model& model, Shader& shader, ViewMode view_mode) {
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
        
        rasterize(clip, shader, framebuffer, view_mode, flat_color);
    }
}

void render_model(TGAImage& framebuffer, const std::string& model_path, const RenderSettings& settings) {
    const vec3 world_light = settings.light_direction;
    const vec4 eye_light = ModelView * vec4{world_light.x(), world_light.y(), world_light.z(), 0.};
    Model model(model_path);

    if (settings.view_mode != ViewMode::Shaded) {
        RandomShader shader(model);
        render_model_with_shader(framebuffer, model, shader, settings.view_mode);
        return;
    }

    switch (settings.shader_mode) {
        case ShaderMode::PhongSpecular: {
            PhongSpecularShader shader(model, settings.ambient, settings.diffuse, eye_light.xyz(), settings.specular, settings.shininess, settings.eye);
            render_model_with_shader(framebuffer, model, shader, settings.view_mode);
            break;
        }
        case ShaderMode::BlinnPhongSpecular: {
            BlinnPhongSpecularShader shader(model, settings.ambient, settings.diffuse, eye_light.xyz(), settings.specular, settings.shininess, settings.eye);
            render_model_with_shader(framebuffer, model, shader, settings.view_mode);
            break;
        }
        default: {
            DiffuseShader shader(model, settings.ambient, settings.diffuse, eye_light.xyz(), settings.eye);
            render_model_with_shader(framebuffer, model, shader, settings.view_mode);
            break;
        }
    }
}

RenderResult render(const RenderSettings& settings) {
    lookat(settings.eye, settings.center, settings.up);                                            // build the ModelView matrix
    init_perspective((settings.eye-settings.center).length());                                     // build the Perspective matrix
    init_viewport(settings.width/16, settings.height/16, settings.width*7/8, settings.height*7/8); // build the Viewport matrix
    init_zbuffer(settings.width, settings.height);

    TGAImage framebuffer(settings.width, settings.height, TGAImage::RGB);
    RenderResult result;

    const auto render_start = std::chrono::steady_clock::now();
    for(const std::filesystem::path& model_path : settings.model_paths) {
        render_model(framebuffer, model_path.string(), settings);
    }

    const auto render_end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> render_duration = render_end - render_start;
    
    result.duration_ms = render_duration.count();
    result.succeeded = framebuffer.write_tga_file(settings.output_path.string());
    return result;
}

RenderResult render_demo(const RenderSettings& settings) {
    lookat(settings.eye, settings.center, settings.up);
    init_perspective((settings.eye-settings.center).length());
    init_viewport(settings.width/16, settings.height/16, settings.width*7/8, settings.height*7/8);
    init_zbuffer(settings.width, settings.height);

    TGAImage framebuffer(settings.width, settings.height, TGAImage::RGB);
    RenderResult result;

    const auto render_start = std::chrono::steady_clock::now();
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
    rasterize(far, far_shader, framebuffer, settings.view_mode, blue);
    rasterize(near, near_shader, framebuffer, settings.view_mode, red);

    const auto render_end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> render_duration = render_end - render_start;

    result.duration_ms = render_duration.count();
    result.succeeded = framebuffer.write_tga_file(settings.output_path.string());
    return result;
}