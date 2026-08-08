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

# big obstacle (SDF) + collision — default is slip, big circle at center
./build/tracers2d_live --circle 115 64 23
./build/tracers2d_live --block 115 64 46 46           # center cx,cy + full size w,h
./build/tracers2d_live --circle 80 64 18 --block 180 64 40 40  # multiple
./build/tracers2d_live --no-obstacle                  # no obstacle (old 0b)
./build/tracers2d_live --coll slip                    # slip (default): wind slides, keep tangent
./build/tracers2d_live --coll bounce                  # elastic: v' = v -2(v·n)n
./build/tracers2d_live --coll stick                   # no-slip visual: stops at wall
./build/tracers2d_live --coll passive                 # push-out only (future LBM guard)
```

Window: dark field `nx*scale × ny*scale` pixels, particles enter as a thin inlet sheet at `x∈[0,1)` and stream L→R at `u0` (`SPD = u0·dt`), recycle at inlet visibly. New particles from top-bar / `KP +/-` also enter at inlet. Obstacles are flat warm-grey silhouettes with crisp outline (no glow/gradient) drawn via `viewer/field_viewer::drawObstacles` from `src/obstacle.h` SDFs. Resize is safe (letterboxes, no crash). Zoom/pan: scroll zooms to cursor, RMB-drag pans, `RST` resets. Title shows `PTS / SPD / Z / FPS / ms / recycled/s / count / coll / obs`.

Controls: `SPACE` pause/resume, `R` reseed, keypad `+/-` add/remove 100 tracers (or top-bar `-500/-100/+100/+500`), `S-/S+` (top-bar or `[/]`) speed `dt*=0.85/1.18`, `Z-/Z+` (top-bar or `-/=`) zoom, `C` cycle coll mode `slip→stick→bounce→passive`, `B` toggle `circle→block→none`, `0`/`RST` reset view, `ESC`/`Q` quit. All obstacles in lattice coords `[0,nx)×[0,ny)` — same SDF later generates `solidMask` for LBM `boundary.h:setB`.

Gate 0b: window opens, constant-speed stream, recycle visible, obstacles deflect via SDF push + mode, stays >55 FPS at 10k points on integrated graphics, `+/-`/`R`/`C`/`B` live, resize does not crash.

### 5. Quick dev loop — `rebuild_and_run_cpu.sh` (macOS + Linux)

From repo root (one command: configure → build → ctest → launch):

```sh
chmod +x rebuild_and_run_cpu.sh
./rebuild_and_run_cpu.sh                              # build + launch defaults
./rebuild_and_run_cpu.sh -- --nx 512 --particles 5000 # pass args to tracers2d_live
./rebuild_and_run_cpu.sh --watch                      # rebuild & relaunch on every src change
./rebuild_and_run_cpu.sh --watch -- -- --particles 20000 --circle 80 64 18
```

Or manually from this directory (same effect):
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j && ctest --test-dir build && ./build/tracers2d_live
```

`--watch` uses `fswatch` on macOS (`brew install fswatch`) or `inotifywait` on Linux (`sudo apt install inotify-tools`), falls back to 1 s poll.

### 6. Clean

```sh
rm -rf build out
```

## What works / what doesn't

- ✅ `Field2D`/`Field3D` SoA + `y*nx+x` + bilinear sampling
- ✅ `TracerSet` advection, inlet recycle, `y` wrap, `add`/`remove`
- ✅ `src/obstacle.h` — SDF `Circle`/`AABB` + `CollMode` `slip|stick|bounce|passive` (Phase 0b visual; 0c keeps passive guard)
- ✅ `tracers2d_headless` + `ctest` green (`advect` overload keeps Gate 0a)
- ✅ `viewer/field_viewer` + `tracers2d_live` — live window, same field/tracer loop, `drawObstacles` + `--circle/--block/--coll/--no-obstacle` + `C`/`B` keys
- ⏳ `lattice.h` / `lbm.h` / `boundary.h` / `voxelize.h` — Phase 0c (real LBM replaces `fillUniform`), not yet

## Next

Phase 0c: `lattice.h`/`units.h`/`lbm.h`/`boundary.h` — replace `fillUniform` with `lbm.step()`; tracers keep `advect(...,obstacles,passive)` as tunneling guard only (active `slip/bounce` removed, `sdf<0→solidMask` drives real `f_opp` bounce-back); viewer keeps same `drawObstacles` API.
