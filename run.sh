#!/bin/bash

set -e

parallel=0
view_mode="shaded"
shader_mode="diffuse"
model_path="obj/diablo3_pose/diablo3_pose.obj"
output_path="framebuffer.tga"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --parallel)
            parallel=1
            shift
            ;;
        --serial)
            parallel=0
            shift
            ;;
        --demo)
            model_path="--demo"
            shift
            ;;
        --fill)
            if [[ $# -lt 2 ]]; then
                echo "--fill expects shaded, flat, intensity, barycentric, or wireframe" >&2
                exit 1
            fi
            view_mode="$2"
            shift 2
            ;;
        --shader)
            if [[ $# -lt 2 ]]; then
                echo "--shader expects random, diffuse, phong, or blinnphong" >&2
                exit 1
            fi
            shader_mode="$2"
            shift 2
            ;;
        --output)
            if [[ $# -lt 2 ]]; then
                echo "--output expects a .tga path" >&2
                exit 1
            fi
            output_path="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: ./run.sh [--serial|--parallel] [--shader random|diffuse|phong|blinnphong] [--fill shaded|flat|intensity|barycentric|wireframe] [--output framebuffer.tga] [--demo|path/to/model.obj]"
            echo
            echo "Defaults:"
            echo "  mode:  --serial"
            echo "  shader: diffuse"
            echo "  fill:  shaded"
            echo "  model: obj/diablo3_pose/diablo3_pose.obj"
            echo "  output: framebuffer.tga"
            exit 0
            ;;
        *)
            model_path="$1"
            shift
            ;;
    esac
done

cmake -S . -B build -DRASTERIZER_PARALLEL="$parallel"
cmake --build build -j
build/not_so_tinyrenderer --shader "$shader_mode" --fill "$view_mode" --output "$output_path" "$model_path"
