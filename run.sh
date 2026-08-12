#!/bin/bash

set -e

parallel=0
fill_mode="flat"
model_path="obj/diablo3_pose/diablo3_pose.obj"

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
                echo "--fill expects flat, intensity, or barycentric" >&2
                exit 1
            fi
            fill_mode="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: ./run.sh [--serial|--parallel] [--fill flat|intensity|barycentric] [--demo|path/to/model.obj]"
            echo
            echo "Defaults:"
            echo "  mode:  --serial"
            echo "  fill:  flat"
            echo "  model: obj/diablo3_pose/diablo3_pose.obj"
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
build/not_so_tinyrenderer --fill "$fill_mode" "$model_path"
