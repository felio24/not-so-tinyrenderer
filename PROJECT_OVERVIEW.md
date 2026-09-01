# TinyRenderer Tutorial Overview

This project follows the TinyRenderer tutorial series:

- Tutorial home: https://haqr.eu/tinyrenderer/
- Original project/code reference: https://github.com/ssloy/tinyrenderer

The goal is to build a small software renderer from scratch in C++. The renderer does not use OpenGL, Vulkan, Metal, DirectX, or a graphics library. It writes pixels directly into a TGA framebuffer, then gradually rebuilds the core ideas behind a 3D graphics pipeline: line drawing, triangle rasterization, depth testing, camera transforms, shading, texture sampling, normal mapping, shadows, ambient occlusion, and stylized rendering.

The tutorial describes the final renderer as roughly 500 lines of core code and estimates about 10-20 hours for students to start producing capable renders. For a deeper learning version where each step is understood and debugged, planning for more time is reasonable.

Repo-specific setup, commands, and implementation notes live in `README.md`.

## Big Picture Pipeline

The renderer gradually becomes this pipeline:

```text
OBJ model data
  -> vertex positions, face indices, normals, UVs, textures
  -> model/world/camera transformations
  -> projection to screen coordinates
  -> triangle rasterization
  -> per-pixel depth test
  -> per-pixel shader
  -> framebuffer.tga
```

At the beginning, most of this is missing. Each tutorial chapter adds one important piece.

## Milestone Plan

### 1. Foundation And Wireframe Rendering

Tutorial page: https://haqr.eu/tinyrenderer/bresenham/

Goal: draw reliable 2D lines, then use them to render OBJ models as wireframes.

Core concepts:

- Framebuffer as a 2D pixel array.
- TGA image output.
- Line drawing from two integer screen-space endpoints.
- Handling shallow, steep, positive, negative, forward, and backward lines.
- OBJ face indices.
- Orthographic projection.

Tasks:

- Fix `line(...)` so it handles all octants.
- Remove or gate noisy debug logging.
- Verify line drawing with small manual triangles.
- Render `african_head.obj` and `diablo3_pose.obj` as wireframes.
- Make `main.cpp` accept an OBJ path from `argv`.

Done when:

- Any two screen points can be connected without gaps or direction bugs.
- A model wireframe appears recognizable in `framebuffer.tga`.
- The model path can be passed from `run.sh`.

Suggested time:

- 2-4 focused sessions.

### 2. Filled Triangles

Tutorial page: https://haqr.eu/tinyrenderer/rasterization/

Goal: move from triangle edges to filled triangles.

Core concepts:

- Triangle filling.
- Scanline rasterization.
- Sorting triangle vertices by screen-space `y`.
- Filling horizontal spans.
- Back-face culling.
- Random or flat colors per triangle.

Tasks:

- Implement a `triangle(...)` function.
- Start with a simple scanline fill.
- Draw a few test triangles before using OBJ models.
- Replace wireframe rendering with filled triangle rendering.
- Add optional back-face culling using triangle normals or screen-space orientation.

Done when:

- Individual triangles fill correctly.
- The model renders as a solid faceted object.
- Back-facing triangles can be skipped.

Suggested time:

- 2-4 sessions.

### 3. Barycentric Coordinates

Tutorial page: https://haqr.eu/tinyrenderer/barycentric/

Goal: use barycentric coordinates as the general way to decide whether a pixel is inside a triangle and interpolate values across the triangle.

Core concepts:

- A point inside a triangle can be represented as weighted contributions from the three vertices.
- Barycentric coordinates sum to 1.
- Negative barycentric coordinates usually mean the point is outside the triangle.
- The same weights can interpolate color, depth, normals, UVs, and other attributes.

Tasks:

- Implement a `barycentric(...)` helper.
- Rewrite triangle filling around a bounding box plus inside-triangle test.
- Interpolate simple colors across a triangle.
- Keep this implementation clean because later features will reuse it heavily.

Done when:

- Triangles fill correctly using barycentric coordinates.
- You can interpolate at least one value across a triangle.

Suggested time:

- 2-3 sessions.

### 4. Hidden Face Removal And Z-Buffer

Tutorial page: https://haqr.eu/tinyrenderer/z-buffer/

Goal: solve overlapping geometry so nearer fragments hide farther fragments.

Core concepts:

- Painter's algorithm is insufficient for arbitrary meshes.
- Depth must be tracked per pixel.
- Barycentric coordinates let you interpolate `z` across a triangle.
- A z-buffer stores the closest depth seen so far for each screen pixel.

Tasks:

- Add a z-buffer separate from the color framebuffer.
- Interpolate depth for each candidate pixel.
- Only draw a pixel if it is closer than the stored depth.
- Output a debug `zbuffer.tga` image if useful.
- Prefer a floating-point depth buffer over an 8-bit image-backed depth buffer.

Done when:

- The model no longer shows back-side triangles through front-side triangles.
- Depth ordering works on `diablo3_pose.obj`.

Suggested time:

- 2-4 sessions.

### 5. Naive Camera And Perspective

Tutorial page: https://haqr.eu/tinyrenderer/camera-naive/

Goal: go beyond pure orthographic projection.

Core concepts:

- Camera movement can be simulated by transforming the scene.
- Rotation matrices change the view direction.
- Perspective projection makes closer objects appear larger.
- Depth values may leave the assumed `[-1, 1]` range after transforms.

Tasks:

- Add basic matrix/vector multiplication if not already present.
- Rotate the model before projecting.
- Add a simple perspective deformation.
- Fix depth range assumptions by using a floating-point z-buffer.
- Make model/camera constants easy to tweak.

Done when:

- Rendering changes visibly when the model/view is rotated.
- Perspective looks different from orthographic projection.
- No clipping/overflow occurs because of shallow depth-buffer assumptions.

Suggested time:

- 2-4 sessions.

### 6. Proper Camera Transform Stack

Tutorial page: https://haqr.eu/tinyrenderer/camera/

Goal: organize transformations like a real graphics pipeline.

Core concepts:

- Object coordinates.
- World coordinates.
- Eye/camera coordinates.
- Clip coordinates.
- Screen coordinates.
- Model, View, Perspective, and Viewport transforms.
- Homogeneous coordinates.

Implementation note - TinyRenderer/OpenGL terminology:

- OpenGL's `Model` matrix corresponds to TinyRenderer's `Model` transform, when present.
- OpenGL's `View` matrix corresponds to TinyRenderer's `View` or `ModelView` transform, usually built from camera parameters such as `eye`, `center`, and `up`.
- OpenGL's `Projection` matrix corresponds to TinyRenderer's `Perspective` transform.
- OpenGL's `glViewport(...)` step corresponds to TinyRenderer's explicit `Viewport` matrix.
- TinyRenderer's `Viewport` matrix is not the camera/view matrix. It maps projected normalized device coordinates, usually `[-1, 1]` in `x` and `y`, into final screen/window pixel coordinates.
- Calls such as `viewport(width/16, height/16, width*7/8, height*7/8)` define an inset render rectangle: the origin is offset by one sixteenth of the framebuffer, and the viewport occupies seven eighths of its width and height.

Implementation note - TinyRenderer/Ray Tracing in One Weekend camera comparison:

- Ray Tracing in One Weekend does not use an MVP transform stack in the same way because it traces rays through pixels instead of rasterizing projected triangles.
- `lookfrom` in Ray Tracing in One Weekend corresponds to TinyRenderer's `eye`: the camera position.
- `lookat` in Ray Tracing in One Weekend corresponds to TinyRenderer's `center`: the world-space point the camera aims at.
- `vup` in Ray Tracing in One Weekend corresponds to TinyRenderer's `up`: an approximate world-space up direction used to control camera roll.
- TinyRenderer's `lookat(...)` is a function name for building the `ModelView`/view matrix from `eye`, `center`, and `up`; Ray Tracing in One Weekend's `lookat` is only the target point.
- TinyRenderer transforms object/world-space vertices into camera space, then projects them to screen pixels. Ray Tracing in One Weekend builds a camera basis and viewport plane in world space, then fires one or more rays from the camera through each pixel.

Implementation note - rasterization matrix math:

- TinyRenderer's final camera code builds a 3x3 matrix from the projected screen-space triangle vertices:

  ```text
  ABC = [ Ax Ay 1 ]
        [ Bx By 1 ]
        [ Cx Cy 1 ]
  ```

- `ABC.det()` is the shoelace formula for the signed projected triangle area, but without the final division by two. In other words, `ABC.det()` is twice this project's `triangle_area(A, B, C)` result.
- The division by two does not matter when all barycentric coordinates are computed as determinant ratios, because the same factor appears in the numerator and denominator and cancels out. It does matter if one side of the ratio uses `triangle_area(...)` and the other side uses `ABC.det()`.
- The check `if (ABC.det() < 1) return;` combines screen-space back-face culling with skipping degenerate or sub-pixel triangles.
- The rasterizer builds `ABC` from screen coordinates because the per-pixel loop asks whether framebuffer pixel `(x, y)` lies inside the projected triangle.
- Depth can still be interpolated from NDC `z` values. The screen-space barycentric weights are valid for this because the viewport transform from NDC `x,y` to screen `x,y` is affine, so it preserves barycentric coordinates.
- Keep each candidate pixel internally consistent: use either the old area-based barycentric coordinates for the inside test, depth interpolation, and coloring, or use the matrix-solved barycentric coordinates for all three. Mixing them can cause visible differences because the area helper currently truncates floating-point screen vertices to integers, while the matrix path keeps the precise projected coordinates.

Tasks:

- Add matrix types if your `vec3` helper is not enough.
- Implement viewport transform.
- Implement perspective matrix/deformation.
- Implement a view/camera transform.
- Compose transforms instead of applying many ad hoc functions.
- Add camera parameters such as `eye`, `center`, and `up`.

Done when:

- The renderer uses a clear transform pipeline.
- Moving camera parameters changes the final image predictably.

Suggested time:

- 3-5 sessions.

### 7. Basic Shading

Tutorial page: https://haqr.eu/tinyrenderer/shading/

Goal: replace random/flat colors with lighting based on surface direction.

Core concepts:

- Triangle normals.
- Light direction.
- Lambert diffuse shading.
- Phong-style local shading as the project grows.
- Shader-like organization, even though everything is CPU-side.

Tasks:

- Compute face normals.
- Add a light direction.
- Shade triangles based on normal dot light direction.
- Refactor rendering so per-pixel shading can evolve.

Done when:

- The model has directional light and shadowed/darker sides.
- The image is more readable than random-color rendering.

Suggested time:

- 2-3 sessions.

### 8. More OBJ Data, Smooth Shading, And Textures

Tutorial page: https://haqr.eu/tinyrenderer/textures/

Goal: stop using only vertex positions. Use normals, UV coordinates, and texture images already present in the model assets.

Core concepts:

- OBJ `v`, `vt`, and `vn` arrays.
- Face entries of the form `vertex/uv/normal`.
- Smooth shading by interpolating normals.
- Texture lookup through UV coordinates.
- Diffuse, specular, and normal-map texture inputs.

Tasks:

- Extend `Model` to parse `vt` texture coordinates.
- Extend `Model` to parse `vn` normals.
- Store per-face indices for vertices, UVs, and normals.
- Load diffuse textures.
- Interpolate UVs per pixel.
- Sample texture color inside the triangle shader.
- Add smooth normal interpolation.
- Add specular texture support if following the full path.

Done when:

- The model renders with its diffuse texture.
- Smooth shading reduces the faceted triangle look.
- The loader clearly separates vertex, UV, and normal index arrays.

Suggested time:

- 4-7 sessions.

### 9. Tangent-Space Normal Mapping

Tutorial page: https://haqr.eu/tinyrenderer/tangent/

Goal: support normal maps that are expressed in tangent space instead of global/world space.

Core concepts:

- Normal maps store per-pixel surface-normal detail.
- Tangent-space normal maps are relative to a local surface frame.
- Each fragment needs a tangent, bitangent, and normal basis.
- UV layout affects tangent frame computation.

Tasks:

- Compute tangent and bitangent vectors per triangle.
- Build a tangent frame from triangle positions and UVs.
- Convert sampled tangent-space normals into world/camera space.
- Use `_nm_tangent.tga` maps.
- Compare against global normal-map behavior.

Done when:

- Diablo-style mirrored/reused UV areas shade correctly.
- Tangent-space normal maps produce believable fine detail.

Suggested time:

- 3-5 sessions.

### 10. Shadow Mapping

Tutorial page: https://haqr.eu/tinyrenderer/shadow/

Goal: add cast shadows using a second depth pass from the light's point of view.

Core concepts:

- Local shading cannot know whether something blocks the light.
- Shadow mapping renders the scene from the light first.
- The first pass stores light-space depth.
- The camera pass compares each fragment against the light-space depth.
- Depth bias is needed to reduce self-shadowing artifacts.

Tasks:

- Render a depth-only pass from the light's view.
- Store the light-space transform.
- During the camera pass, transform each fragment into light screen space.
- Compare against the shadow map.
- Add a small bias to reduce acne/z-fighting.

Done when:

- Objects can cast visible shadows onto other objects or the floor.
- Shadow artifacts are understood and controlled enough for the tutorial output.

Suggested time:

- 4-6 sessions.

### 11. Ambient Occlusion / Indirect Lighting Approximation

Tutorial page: https://haqr.eu/tinyrenderer/ssao/

Goal: add a global-illumination approximation so creases and contact areas feel less flat.

Core concepts:

- Direct lighting alone misses indirect-light effects.
- Ambient occlusion darkens areas where surrounding geometry blocks ambient light.
- Screen-space ambient occlusion works from depth/screen-space information.

Tasks:

- Produce the data needed for AO, likely depth and/or position information.
- Implement the tutorial's AO pass.
- Tune radius, sampling, and intensity.
- Composite AO into final shading.

Done when:

- Creases, contact shadows, and recessed areas read more clearly.
- AO can be toggled or compared with a non-AO render.

Suggested time:

- 3-6 sessions.

### 12. Bonus: Toon Shading

Tutorial page: https://haqr.eu/tinyrenderer/toon/

Goal: make a stylized non-photorealistic render.

Core concepts:

- Quantized lighting bands.
- Contour emphasis.
- Sobel edge detection as an image-space post-process.

Tasks:

- Quantize diffuse intensity into a few bands.
- Add an edge-detection pass.
- Combine bands and outlines into a toon-style image.

Done when:

- The same model can render in a cartoon/cel-shaded style.

Suggested time:

- 2-4 sessions.

## Suggested Schedule Shapes

### Compact 4-Week Plan

Use this if you can do several focused sessions per week.

Week 1:

- Finish line drawing.
- Finish wireframe OBJ rendering.
- Implement filled triangles.

Week 2:

- Implement barycentric triangle rasterization.
- Add z-buffering.
- Render solid depth-correct models.

Week 3:

- Add naive camera rotation and perspective.
- Build a proper transform pipeline.
- Add basic diffuse shading.

Week 4:

- Parse UVs/normals.
- Add texture sampling.
- Add smooth shading.
- Start normal mapping if time remains.

### Comfortable 8-Week Plan

Use this if you want time to understand and write notes.

Week 1:

- Wireframe rendering and line drawing.

Week 2:

- Triangle filling and barycentric coordinates.

Week 3:

- Z-buffer and hidden-surface removal.

Week 4:

- Camera basics, perspective, and transform cleanup.

Week 5:

- Basic lighting and shader structure.

Week 6:

- OBJ normals, UVs, diffuse textures, smooth shading.

Week 7:

- Normal mapping and tangent-space normal mapping.

Week 8:

- Shadow mapping, AO, toon shading, or polish depending on interest.

## Recommended Commit Boundaries

Good commits should each produce a visible, testable step.

1. `Add reliable line rasterizer`
2. `Add OBJ loader and wireframe model rendering`
3. `Add filled triangle rasterization`
4. `Add barycentric triangle rasterizer`
5. `Add z-buffer depth testing`
6. `Add camera rotation and perspective projection`
7. `Add transform matrix pipeline`
8. `Add diffuse lighting`
9. `Parse OBJ UVs and normals`
10. `Add diffuse texture sampling`
11. `Add smooth shading`
12. `Add normal mapping`
13. `Add tangent-space normal mapping`
14. `Add shadow mapping`
15. `Add ambient occlusion`
16. `Add toon shading`
