cmake_minimum_required(VERSION 3.16)
project(cpu-lbm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings
if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core library — no graphics deps in Phase 0c solver
add_library(cpu_lbm_core STATIC
  src/field.cpp
  src/tracers.cpp
  src/lbm.cpp
  src/boundary.cpp
  src/probes.cpp
  src/io.cpp
)
target_include_directories(cpu_lbm_core PUBLIC src)

# Headless Phase 0a app: uniform field + tracers, no window, CI-friendly
add_executable(tracers2d_headless
  apps/tracers2d_headless.cpp
)
target_link_libraries(tracers2d_headless PRIVATE cpu_lbm_core)

# Phase 0c batch app
add_executable(cyl2d_batch
  apps/cyl2d_batch.cpp
)
target_link_libraries(cyl2d_batch PRIVATE cpu_lbm_core)

# Tests — header-only, no external framework (keeps offline builds green)
enable_testing()

add_executable(test_field tests/test_field.cpp)
target_link_libraries(test_field PRIVATE cpu_lbm_core)
add_test(NAME test_field COMMAND test_field)

add_executable(test_tracers tests/test_tracers.cpp)
target_link_libraries(test_tracers PRIVATE cpu_lbm_core)
add_test(NAME test_tracers COMMAND test_tracers)

add_executable(test_lattice tests/test_lattice.cpp)
target_link_libraries(test_lattice PRIVATE cpu_lbm_core)
add_test(NAME test_lattice COMMAND test_lattice)

add_executable(test_units tests/test_units.cpp)
target_link_libraries(test_units PRIVATE cpu_lbm_core)
add_test(NAME test_units COMMAND test_units)

add_executable(test_lbm tests/test_lbm.cpp)
target_link_libraries(test_lbm PRIVATE cpu_lbm_core)
add_test(NAME test_lbm COMMAND test_lbm)

add_executable(test_boundary tests/test_boundary.cpp)
target_link_libraries(test_boundary PRIVATE cpu_lbm_core)
add_test(NAME test_boundary COMMAND test_boundary)

# Phase 0b viewer — GLFW + OpenGL, linked only by *_live apps so core stays headless
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/viewer/field_viewer.cpp")
  find_package(OpenGL REQUIRED)

  # Prefer system GLFW (brew install glfw) for offline / fast builds; fallback to FetchContent
  find_package(glfw3 QUIET)
  if (NOT glfw3_FOUND)
    include(FetchContent)
    FetchContent_Declare(glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG 3.4
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
  endif()

  add_library(cpu_lbm_viewer STATIC viewer/field_viewer.cpp)
  target_include_directories(cpu_lbm_viewer PUBLIC src viewer)
  if (APPLE)
    target_compile_definitions(cpu_lbm_viewer PUBLIC GL_SILENCE_DEPRECATION)
  endif()
  # OpenGL target name differs by CMake version/platform
  if (TARGET OpenGL::GL)
    target_link_libraries(cpu_lbm_viewer PUBLIC cpu_lbm_core glfw OpenGL::GL)
  else()
    target_link_libraries(cpu_lbm_viewer PUBLIC cpu_lbm_core glfw OpenGL::OpenGL)
  endif()

  add_executable(tracers2d_live apps/tracers2d_live.cpp)
  target_link_libraries(tracers2d_live PRIVATE cpu_lbm_viewer)
  if (APPLE)
    target_compile_definitions(tracers2d_live PRIVATE GL_SILENCE_DEPRECATION)
  endif()

  # LBM live viewer — same viewer, LBM-driven
  add_executable(cyl2d_live
    apps/cyl2d_live.cpp
  )
  target_link_libraries(cyl2d_live PRIVATE cpu_lbm_viewer)
  if (APPLE)
    target_compile_definitions(cyl2d_live PRIVATE GL_SILENCE_DEPRECATION)
  endif()
endif()
