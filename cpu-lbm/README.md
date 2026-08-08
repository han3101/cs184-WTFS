# cpu-lbm — Phase 0a + 0b (headless + viewer)

Headless field/grid/tracer core with a thin live viewer on top. No physics yet — uniform flow `u=(0.05,0)` that Phase 0c will replace with LBM. Viewer is a neutral dark window with warm-white points; no gradients/glow.

## cpu-run — build & run (what works right now)

### Prerequisites

CMake ≥3.16 + C++17 compiler. For the viewer (`tracers2d_live`) also: GLFW + OpenGL.

macOS:
```sh
xcode-select --install
brew install cmake glfw   # glfw optional — CMake will fetch 3.4 if not found
```

Linux (Debian/Ubuntu):
```sh
sudo apt update && sudo apt install -y cmake build-essential libglfw3-dev libgl-dev
```

### 1. Configure

```sh
# from this directory (cpu-lbm/)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# from repo root: cmake -S cpu-lbm -B cpu-lbm/build -DCMAKE_BUILD_TYPE=Release
```

`Field`/`Tracers` need no deps — offline builds are green. `viewer/` auto-enables when `viewer/field_viewer.cpp` exists; otherwise only `tracers2d_headless` + tests are built.

### 2. Build

```sh
cmake --build build -j
```

Artifacts:
- `build/tracers2d_headless` — headless loop, CI-friendly (always)
- `build/test_field`, `build/test_tracers` — unit tests (always)
- `build/tracers2d_live` — live window (only when GLFW/OpenGL found)

### 3. Test

```sh
ctest --test-dir build --output-on-failure
# verbose:
./build/test_field; ./build/test_tracers
```

All `PASS`, 0 failures.

### 4a. Run headless (Gate 0a)

```sh
./build/tracers2d_headless --steps 2000
./build/tracers2d_headless --nx 256 --ny 128 --particles 10000 --steps 5000
mkdir -p out && ./build/tracers2d_headless --nx 256 --ny 128 --particles 10000 --steps 5000 --csv out/tracers.csv
./build/tracers2d_headless --u0 0.08 --dt 1.0 --seed 42 --steps 2000
```

Columns `step,avg_x,avg_y,recycled,total_recycled` every 10 steps. Gate: exits `0`, stable count, `total_recycled >0` when `steps > nx/u0`.

### 4b. Run viewer (Gate 0b)

```sh
./build/tracers2d_live
./build/tracers2d_live --nx 256 --ny 128 --particles 10000 --scale 3
./build/tracers2d_live --nx 512 --ny 256 --particles 20000 --u0 0.06 --scale 2
```

Window: dark field `157×`? No — `nx*scale × ny*scale` pixels, particles stream L→R at `u0`, recycle at inlet visibly. Resize is safe (letterboxes, no crash). Title shows `FPS / ms / recycled/s / count`.

Controls: `SPACE` pause/resume, `R` reseed, `+/-` (or `KP +/-`) add/remove 100 tracers (count decoupled), `ESC`/`Q` quit.

Gate 0b: window opens, constant-speed stream, recycle visible, stays >55 FPS at 10k points on integrated graphics, `+/-` and `R` live, resize does not crash.

### 5. Clean

```sh
rm -rf build out
```

## What works / what doesn't

- ✅ `Field2D`/`Field3D` SoA + `y*nx+x` + bilinear sampling
- ✅ `TracerSet` advection, inlet recycle, `y` wrap, `add`/`remove`
- ✅ `tracers2d_headless` + `ctest` green
- ✅ `viewer/field_viewer` + `tracers2d_live` — live window, same field/tracer loop
- ⏳ `lattice.h` / `lbm.h` / `boundary.h` / `obstacle.h` / `voxelize.h` — Phase 0c (real LBM replaces `fillUniform`), not yet

## Next

Phase 0c: `lattice.h`/`units.h`/`lbm.h`/`boundary.h` — replace `fillUniform` with `lbm.step()`, tracers unchanged; viewer keeps the same API.
