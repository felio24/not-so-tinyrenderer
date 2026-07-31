#!/bin/bash

#model_path="${1:-obj/african_head/african_head.obj}"
model_path="${1:-obj/diablo3_pose/diablo3_pose.obj}"

cmake --build build -j &&
build/not_so_tinyrenderer "$model_path"
