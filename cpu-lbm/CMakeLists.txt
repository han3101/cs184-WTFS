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

# Core library — no graphics deps, no external fetches in Phase 0a
add_library(cpu_lbm_core STATIC
  src/tracers.cpp
)
target_include_directories(cpu_lbm_core PUBLIC src)

# Headless Phase 0a app: uniform field + tracers, no window, CI-friendly
add_executable(tracers2d_headless
  apps/tracers2d_headless.cpp
)
target_link_libraries(tracers2d_headless PRIVATE cpu_lbm_core)

# Tests — header-only, no external framework in Phase 0a (keeps offline builds green)
enable_testing()

add_executable(test_field tests/test_field.cpp)
target_link_libraries(test_field PRIVATE cpu_lbm_core)
add_test(NAME test_field COMMAND test_field)

add_executable(test_tracers tests/test_tracers.cpp)
target_link_libraries(test_tracers PRIVATE cpu_lbm_core)
add_test(NAME test_tracers COMMAND test_tracers)

# Optional: viewer target will be added in Phase 0b (GLFW + OpenGL)
# if you try to build tracers2d_live before 0b, CMake will error with a clear message
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/viewer/field_viewer.cpp")
  find_package(OpenGL REQUIRED)
  include(FetchContent)
  FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )
  FetchContent_MakeAvailable(glfw)
  add_library(cpu_lbm_viewer STATIC viewer/field_viewer.cpp)
  target_include_directories(cpu_lbm_viewer PUBLIC src viewer)
  target_link_libraries(cpu_lbm_viewer PUBLIC cpu_lbm_core glfw OpenGL::GL)

  add_executable(tracers2d_live apps/tracers2d_live.cpp)
  target_link_libraries(tracers2d_live PRIVATE cpu_lbm_viewer)
endif()
