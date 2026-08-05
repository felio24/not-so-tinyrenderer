cmake_minimum_required(VERSION 3.12...3.26)

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT (CMAKE_BUILD_TYPE OR DEFINED ENV{CMAKE_BUILD_TYPE}))
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Release default")
endif()

project(not_so_tinyrenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(RASTERIZER_PARALLEL 0 CACHE STRING "Set to 1 to enable OpenMP parallel rasterization")
set_property(CACHE RASTERIZER_PARALLEL PROPERTY STRINGS 0 1)
if(NOT RASTERIZER_PARALLEL MATCHES "^[01]$")
  message(FATAL_ERROR "RASTERIZER_PARALLEL must be set to 0 or 1")
endif()

option(iwyu "Run include-what-you-use")
if(iwyu)
  find_program(IWYU_EXE NAMES include-what-you-use REQUIRED)
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_EXE})
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|Intel")
  add_compile_options(-Wall)
endif()

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  foreach(openmp_prefix /opt/homebrew/opt/libomp /usr/local/opt/libomp)
    if(EXISTS "${openmp_prefix}")
      list(APPEND CMAKE_PREFIX_PATH "${openmp_prefix}")
    endif()
  endforeach()
endif()

find_package(OpenMP COMPONENTS CXX)

set(SOURCES main.cpp tgaimage.cpp model.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE RASTERIZER_PARALLEL=${RASTERIZER_PARALLEL})

if(RASTERIZER_PARALLEL AND NOT OpenMP_CXX_FOUND AND APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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

target_link_libraries(${PROJECT_NAME} PRIVATE $<$<AND:$<BOOL:${RASTERIZER_PARALLEL}>,$<BOOL:${OpenMP_CXX_FOUND}>>:OpenMP::OpenMP_CXX>)

file(GENERATE OUTPUT .gitignore CONTENT "*")
