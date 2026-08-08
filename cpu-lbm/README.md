# cpu-lbm — Phase 0a scaffold

Headless minimal simulator. No physics yet — proves the field/grid/tracer loop that Phase 0c will drive with LBM.

## cpu-run — build & run the headless simulator (what works right now)

Phase 0a is the only shippable slice. It builds with just a C++17 compiler + CMake — no GLFW, no OpenGL, no LBM yet. It owns a `Field2D` (SoA `ux,uy`, flat index `y*nx+x`) and streams `TracerSet` particles through a uniform flow `u=(0.05,0)` with bilinear sampling and inlet recycle.

### 1. Configure

```sh
# from the cpu-lbm/ directory
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

No `FetchContent` fires in Phase 0a — offline builds are green.

### 2. Build

```sh
cmake --build build -j
```

Artifacts:
- `build/tracers2d_headless` — headless loop, CI-friendly
- `build/test_field`, `build/test_tracers` — unit tests

### 3. Test

```sh
ctest --test-dir build --output-on-failure
# or run directly for verbose PASS/FAIL lines:
./build/test_field
./build/test_tracers
```

Expected: all `PASS`, 0 failures. Covers flat-index round-trip, `f[q*ncells+idx]`-ready `y*nx+x` formula, clamp, bilinear sampling, recycle after `nx/u` steps, `add`/`remove`, zero-field, and `y` wrapping.

### 4. Run

```sh
# minimal — 2000 steps, CSV to stdout (Gate 0a)
./build/tracers2d_headless --steps 2000

# custom grid / particle count
./build/tracers2d_headless --nx 256 --ny 128 --particles 10000 --steps 5000

# also dump CSV to file (+ still prints to stdout)
mkdir -p out
./build/tracers2d_headless --nx 256 --ny 128 --particles 10000 --steps 5000 --csv out/tracers.csv

# tweak velocity / timestep / seed
./build/tracers2d_headless --u0 0.08 --dt 1.0 --seed 42 --steps 2000
```

Columns: `step,avg_x,avg_y,recycled,total_recycled` — sampled every 10 steps + final step. Gate 0a: exits `0`, `avg_x` marches monotonically (modulo recycling), `total_recycled > 0` when `steps > nx/u0`, and particle count stays stable.

### 5. Clean

```sh
rm -rf build out
# or rebuild clean:
cmake --build build --target clean
```

## What works / what doesn't (Phase 0a)

- ✅ `Field2D` / `Field3D` indexing + bilinear sampling
- ✅ `TracerSet` advection, inlet recycle, `y` wrap, `add`/`remove`
- ✅ `tracers2d_headless` headless loop + CSV, `ctest` green
- ⏳ `viewer/` + `tracers2d_live` — Phase 0b (GLFW window) not yet built; `CMakeLists.txt` will auto-enable it when `viewer/field_viewer.cpp` lands
- ⏳ `lattice.h` / `lbm.h` / `boundary.h` / `obstacle.h` / `voxelize.h` — Phase 0c (real LBM replaces `fillUniform`), not yet

## Next

- Phase 0b: add `viewer/` (GLFW + GL) and `apps/tracers2d_live.cpp` — same loop with a window.
- Phase 0c: add `lattice.h`/`units.h`/`lbm.h`/`boundary.h` — replace `fillUniform` with `lbm.step()`, tracers unchanged.
