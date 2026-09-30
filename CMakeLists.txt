cmake_minimum_required(VERSION 3.14...3.26)

project(not_so_tinyrenderer LANGUAGES CXX)

# Project configuration
get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT (CMAKE_BUILD_TYPE OR DEFINED ENV{CMAKE_BUILD_TYPE}))
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Release default")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(RASTERIZER_PARALLEL 0 CACHE STRING "Set to 1 to enable OpenMP parallel rasterization")
set_property(CACHE RASTERIZER_PARALLEL PROPERTY STRINGS 0 1)
if(NOT RASTERIZER_PARALLEL MATCHES "^[01]$")
  message(FATAL_ERROR "RASTERIZER_PARALLEL must be set to 0 or 1")
endif()

option(iwyu "Run include-what-you-use")
if(iwyu)
  find_program(IWYU_EXE NAMES include-what-you-use REQUIRED)
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXE}")
endif()

# Optional parallel rasterization
if(RASTERIZER_PARALLEL)
  if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    foreach(openmp_prefix /opt/homebrew/opt/libomp /usr/local/opt/libomp)
      if(EXISTS "${openmp_prefix}")
        list(APPEND CMAKE_PREFIX_PATH "${openmp_prefix}")
      endif()
    endforeach()
  endif()

  find_package(OpenMP COMPONENTS CXX)

  if(NOT OpenMP_CXX_FOUND AND APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    find_path(OpenMP_CXX_INCLUDE_DIR omp.h
      PATHS /opt/homebrew/opt/libomp/include /usr/local/opt/libomp/include
    )
    find_library(OpenMP_CXX_LIBOMP_LIBRARY omp
      PATHS /opt/homebrew/opt/libomp/lib /usr/local/opt/libomp/lib
    )

    if(OpenMP_CXX_INCLUDE_DIR AND OpenMP_CXX_LIBOMP_LIBRARY)
      add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
      target_compile_options(OpenMP::OpenMP_CXX INTERFACE -Xpreprocessor -fopenmp)
      target_include_directories(OpenMP::OpenMP_CXX INTERFACE "${OpenMP_CXX_INCLUDE_DIR}")
      target_link_libraries(OpenMP::OpenMP_CXX INTERFACE "${OpenMP_CXX_LIBOMP_LIBRARY}")
      set(OpenMP_CXX_FOUND TRUE)
    endif()
  endif()
endif()

# GUI dependencies
include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b
)

FetchContent_MakeAvailable(glfw imgui)
find_package(OpenGL REQUIRED)

add_library(imgui_backend STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_backend PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_backend PUBLIC
  glfw
  OpenGL::GL
)

# Application
add_executable(${PROJECT_NAME}
  main.cpp
  gui.cpp
  model.cpp
  modelgroup.cpp
  notverygl.cpp
  renderer.cpp
  tgaimage.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  RASTERIZER_PARALLEL=${RASTERIZER_PARALLEL}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  imgui_backend
)

if(RASTERIZER_PARALLEL AND OpenMP_CXX_FOUND)
  target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|Intel")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
endif()

if(APPLE)
  target_compile_definitions(imgui_backend PRIVATE GL_SILENCE_DEPRECATION)
  target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Keep generated build directories out of Git even when a custom name is used.
file(GENERATE OUTPUT .gitignore CONTENT "*")
