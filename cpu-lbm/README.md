# cpu-lbm — Phase 0a + 0b + 0c (headless + viewer + LBM)

Headless field/grid/tracer core with a thin live viewer on top, now with a real D2Q9 Lattice Boltzmann solver underneath. Viewer is a neutral dark window with warm-white points; no gradients/glow.

**Which app do I run?** Two families, and they look completely different:

| App | Field | Use it for |
|---|---|---|
| `tracers2d_headless`, `tracers2d_live` | **uniform stub** `u=(0.05,0)` | Gate 0a/0b regression. Solver changes have *no effect* here. |
| `cyl2d_batch`, `cyl2d_live` | **LBM** (D2Q9) | Gate 0c/A3. This is where physics shows up. |

If you changed anything in `lbm.cpp` / `boundary.cpp` / `lattice.h` and want to see it, run **`cyl2d_live`** — `tracers2d_live` never calls the solver.

## cpu-run — build & run

### Prerequisites

CMake ≥3.16 + C++17 compiler. For the viewers (`*_live`) also: GLFW + OpenGL.

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

The solver needs no deps — offline builds are green. `viewer/` auto-enables when `viewer/field_viewer.cpp` exists; otherwise only the headless apps + tests are built.

> **If `cmake` fails with `Could not find CMAKE_ROOT`**, the `cmake` first on your `PATH` is a broken install (e.g. an unbuilt source tree under `~/apps/`). Use the working one explicitly: `/usr/local/bin/cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`.

### 2. Build

```sh
cmake --build build -j
```

Artifacts:
- `build/tracers2d_headless` — Phase 0a uniform-field loop, CI-friendly (always)
- `build/cyl2d_batch` — Phase 0c LBM cylinder, headless, prints Cd/St (always)
- `build/test_field`, `test_tracers`, `test_lattice`, `test_units`, `test_lbm`, `test_boundary` (always)
- `build/tracers2d_live` — Phase 0b uniform-field window (only when GLFW/OpenGL found)
- `build/cyl2d_live` — Phase 0c LBM window (only when GLFW/OpenGL found)

Nothing lands in `build/` if any target fails to compile — check the tail of the build log rather than assuming a stale binary is current.

### 3. Test

```sh
ctest --test-dir build --output-on-failure
# individually:
./build/test_lbm; ./build/test_boundary
```

Six suites. `test_lbm` carries the physics gates: closed-box mass conservation and no-slip decay, plus the Poiseuille profile check (Gate A1) which prints its measured relative L2.

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
./build/tracers2d_live --nx 256 --ny 128 --batch 300 --interval 0.30 --scale 3
./build/tracers2d_live --batch 500 --freq 0.20 --stream            # continuous stream mode
./build/tracers2d_live --nx 512 --ny 256 --batch 500 --interval 0.25 --u0 0.06 --scale 2

# big obstacle (SDF) + collision — default is slip, big circle at center
./build/tracers2d_live --circle 115 64 23
./build/tracers2d_live --block 115 64 46 46           # center cx,cy + full size w,h
./build/tracers2d_live --circle 80 64 18 --block 180 64 40 40  # multiple
./build/tracers2d_live --no-obstacle                  # no obstacle
./build/tracers2d_live --coll slip                    # slip (default): wind slides, keep tangent
./build/tracers2d_live --coll bounce                  # elastic: v' = v -2(v·n)n
./build/tracers2d_live --coll stick                   # no-slip visual: stops at wall
./build/tracers2d_live --coll passive                 # push-out only (future LBM guard)
```

Window: dark field `nx*scale × ny*scale` pixels, particles emit in clean staggered pulses (default **300 particles every 0.30 seconds**) entering as inlet sheets at `x∈[0,1)` that stream L→R at `u0` (`SPD = u0·dt`) around obstacles and exit the domain cleanly.

Live controls:
- **PULSE/STREAM**: toggle between periodic pulsed wave packets and continuous stream
- **BATCH - / +** (or `P / Shift+P`): change particles per pulse (±50 particles)
- **FREQ - / +** (or `I / Shift+I`): change interval between pulses (±0.05s)
- **BURST** (or `E`): emit an immediate batch of particles
- **SPD - / +** (or `[ / ]`): flow speed scale
- **Mouse / Trackpad**: 1-finger / left-drag pans with 1:1 cursor lock; 2-finger scroll zooms at cursor; Shift-scroll pans
- **SPACE** pause/resume, **R** clear & reseed, **C** cycle collision mode, **B** toggle obstacle, **0 / RESET** reset view, **ESC / Q** quit.

### 4c. Run LBM headless (Gate 0c / A3)

```sh
./build/cyl2d_batch                                   # defaults: 400x80, Re=100, u0=0.05, 20000 steps
./build/cyl2d_batch --re 100 --steps 20000 --strict   # CI form: nonzero exit if the gate misses
mkdir -p out && ./build/cyl2d_batch --csv out/cyl2d.csv --raw out/cyl2d.raw
./build/cyl2d_batch --nx 800 --ny 160 --re 100        # finer: D doubles to 40 cells
./build/cyl2d_batch --uniform                         # skip LBM entirely (0a/0b regression)
```

Prints `step N Cd=… St=…` every 2000 steps, then a `FINAL` line and `gate A3 PASSED` / `gate A3 FAILED`.
Gate targets are St ∈ [0.164, 0.167] and Cd ∈ [1.32, 1.36]; `--strict` turns a miss into exit code 1 so `ctest`/CI can depend on it. Without `--strict` it reports and exits 0.

`--csv` writes `step,drag,lift,Cd,Cl,St`; `--raw` dumps `ux`, `uy`, `rho` as three contiguous float arrays — that's the file Phase C1 diffs the GPU port against.

If the run diverges you get `gate A3 FAILED … — solution diverged` rather than a silent `nan`. Divergence usually means τ is too close to 0.5: raise resolution (`--ny`) so D grows, don't lower `u0`. See `docs/units.md`.

### 4d. Run LBM viewer (Gate 0c, live)

```sh
./build/cyl2d_live                                    # 400x80, Re=100, cylinder at 30% span
./build/cyl2d_live --re 150 --scale 3
./build/cyl2d_live --nx 600 --ny 120 --re 100 --u0 0.05
./build/cyl2d_live --circle 120 40 10                 # explicit obstacle: cx cy r
./build/cyl2d_live --block 120 40 20 20               # cx cy w h
./build/cyl2d_live --no-obstacle                      # empty channel — should relax to Poiseuille
./build/cyl2d_live --uniform                          # LBM off, uniform field (Gate 0b regression)
```

Controls: **SPACE** pause/resume, **ESC** quit.

What you should see: tracers accelerate around the cylinder shoulders, a symmetric recirculation pair forms behind it within the first few thousand steps, then the wake destabilises into an alternating Kármán vortex street. If the tracers just stream straight through as if nothing is there, you are almost certainly running `tracers2d_live` (uniform field) rather than `cyl2d_live`.

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
- ✅ `lattice.h` — D2Q9 weights/opposites (D3Q19 defined, unused until Phase B)
- ✅ `units.h` + `docs/units.md` — `LbUnits::fromRe` → ν, τ, dx, dt with a τ>0.51 stability predicate
- ✅ `lbm.h/.cpp` — BGK collide + pull-stream, halfway bounce-back fused into the stream
- ✅ `boundary.h/.cpp` — Zou-He velocity inlet, zero-gradient outlet, SDF→solid mask
- ✅ `probes.h/.cpp` — Ladd momentum-exchange drag/lift, wake probe + zero-crossing Strouhal
- ✅ `io.h/.cpp` — raw field dump, field CSV, probe-history CSV
- ⏳ `io::writePNG` — stub returning `false`; frame export is Phase D
- ⏳ `voxelize.h` / `obj.h` — deferred to Phase B with the 3D path (no car mesh yet)

### Buffer contract (read before touching `lbm.cpp`)

`f` is the canonical state; there is no ping-pong swap.

```
collideBGK:  reads f       → writes collided into f_next
streamPull:  reads f_next  → writes streamed  into f     (+ fused bounce-back)
boundary::applyAll:  fixes up inlet/outlet columns of f
macroscopic():       derives rho/ux/uy from f
```

No-slip is handled entirely inside `streamPull`: when the pull source is solid *or* outside the domain, the cell takes its own collided population from the opposite direction. That covers obstacle surfaces and the north/south channel walls in one branch — there is deliberately no separate bounce-back pass, and adding one will double-reflect.

## Next

Phase A1 → A2 as explicit regression (Poiseuille already lives in `test_lbm`; cavity at Re=1000 vs Ghia et al. still to write), then Phase B for D3Q19 + the OBJ voxelizer.
