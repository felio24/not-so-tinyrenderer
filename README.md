# Not So TinyRenderer Implementation Notes

This page tracks this repo's actual setup, current implementation state, and local workflow. The broader TinyRenderer tutorial roadmap lives in `PROJECT_OVERVIEW.md`.

## Current State

Already started:

- TGA framebuffer output.
- Manual pixel setting.
- Custom line drawing experiments.
- `vec3` math type.
- `Model` loader for OBJ vertex positions and face vertex indices.
- Flattened face-index storage through `facet_vrt`.
- Orthographic projection from 3D model coordinates to 2D screen coordinates.
- Filled triangle rasterization using a bounding box and barycentric-style area tests.
- Optional OpenMP parallelization for the triangle fill loop.
- Render timing through `std::clog`.

Known current issue:

- Parallelizing inside each triangle can be slower than serial rendering because the OpenMP setup cost is paid for many small triangle loops.

## Build And Run

The easiest entry point is `run.sh`. It configures CMake, builds the project, then runs the renderer.

```bash
./run.sh
```

Defaults:

- Rasterization mode: serial.
- Model: `obj/diablo3_pose/diablo3_pose.obj`.
- Output image: `framebuffer.tga`.

Useful variants:

```bash
./run.sh --demo
./run.sh obj/african_head/african_head.obj
./run.sh --fill intensity --demo
./run.sh --fill barycentric obj/diablo3_pose/diablo3_pose.obj
./run.sh --parallel obj/diablo3_pose/diablo3_pose.obj
./run.sh --serial obj/diablo3_pose/diablo3_pose.obj
```

Show script usage:

```bash
./run.sh --help
```

## Build Options

Parallel rasterization is controlled by the CMake cache variable `RASTERIZER_PARALLEL`.

```bash
cmake -S . -B build -DRASTERIZER_PARALLEL=1
cmake --build build -j
build/not_so_tinyrenderer obj/diablo3_pose/diablo3_pose.obj
```

Use `RASTERIZER_PARALLEL=0` to return to the serial baseline.

On macOS, OpenMP uses CPU threads through Homebrew `libomp`; it does not use the GPU.

## Fill Modes

Triangle coloring can be switched without changing the rasterizer code:

- `flat`: one random color per triangle.
- `intensity`: interpolate a scalar value across the triangle and display it as grayscale.
- `barycentric`: show barycentric coordinates as RGB-style channel gradients. The storage order is BGRA, so the helper handles the channel packing.

```bash
./run.sh --fill flat
./run.sh --fill intensity
./run.sh --fill barycentric
```

## Runtime Output

At startup, the renderer logs whether OpenMP rasterization is active:

```text
OpenMP rasterization disabled
```

or:

```text
OpenMP enabled with N threads
```

After rendering, it logs the render duration:

```text
Render time: <value> ms
```

The timer measures the render call only. Writing `framebuffer.tga` is outside the measured section.

## Practical Notes

- Keep `framebuffer.tga` and optionally `zbuffer.tga` as the main output artifacts.
- Prefer small visual tests before rendering full models.
- Each renderer milestone should have one known-good model render.
- Avoid committing while debug-only triangle highlighting or noisy `std::clog` output is still in the main path.
- Keep setup/build convenience work in scripts or docs so rasterizer code remains focused on rendering concepts.
