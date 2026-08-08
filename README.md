# WTFS — Wind Tunnel Fluid Simulator

> Revival of `CS184 Assignment 4: Cloth Simulation` (Sp23) repurposed as a particle-based fluid / wind tunnel. Original spec: [Assignment 4: Cloth Simulation](https://cs184.eecs.berkeley.edu/sp23/docs/proj4).

## What this is

This repo started as the Berkeley cloth mass-spring simulator (`Cloth` + `PointMass` + `Spring` + `ClothMesh` + `Sphere`/`Plane` collisions, `nanogui`/`CGL` viewer). It is being converted into a **Lagrangian fluid simulator** displayed in a wind-tunnel context.

The current `windsim` executable (`src/CMakeLists.txt`) builds:

* `src/cloth.cpp` / `src/cloth.h` — now hosts the fluid solver (despite the name `Cloth`)
* `src/pointMass.h` — extended to an SPH particle
* `src/clothSimulator.cpp` / `src/clothSimulator.h` — viewer, now renders particles as `GL_POINTS`
* `src/grid.cpp` / `src/grid.h` and `src/navierStokes.cpp` / `src/navierStokes.h` — stubs for a future Eulerian grid solver
* `src/main.cpp` — window title `Wind Tunnel Simulator`, continuous inflow loop

## How it (tries to) work

Implements **Position Based Fluids** (Macklin & Müller 2013) on top of the old Verlet integrator:

1.  **Integration:** `Cloth::simulate()` in `src/cloth.cpp:119` applies external accelerations (gravity `= (0,0,4.5)` in `src/clothSimulator.h:63`) via Verlet (`pos += (1-damping)*(pos-last_pos) + a*dt²`).
2.  **Neighbor search:** `build_spatial_map()` + `hash_box(pos, h)` spatial hash (`unordered_map<int, vector<PointMass*>>`) with `h = 0.12` (`src/cloth.h:61`), `set_neighbors()` scans 27 cells.
3.  **Constraint solve (2 iterations):** `calculate_lambda()` (Eq. 11, density constraint `C = rho/rho0 - 1` with `poly6` kernel) -> `calculate_delta_p()` (Eq. 14 with `spiky` gradient and `s_corr = -k*(W(r)/W(dq))^n` where `k=1e-5, n=4, dq=0.2h`) -> `pos += delta_p`. Relaxation `epsilon = 160000` (`src/cloth.h:66`).
4.  **Optional:** `viscosity()` (XSPH, `c=0.01`), `calculate_omega()` / `vorticity()` (`vorticity_eps=0.0002`) — currently commented out in the solve loop.
5.  **Collisions & culling:** `Sphere`/`Plane::collide()` per particle, plus deletion of particles outside `5x5x5` box.
6.  **Inflow:** `src/main.cpp:499-500` calls `cloth.buildGrid()` every 10 frames while unpaused to inject new particles — the "wind tunnel" source.

Kernels in `src/cloth.h:91-105`: `poly6(r,h) = 315/(64πh⁹)(h²-r²)³`, `spiky(r,h) = -r * 45/(πh⁶|r|)(h-|r|)²`.

Renderer in `src/clothSimulator.cpp:364-417` (`drawNormals`/`drawPhong`) uploads `position` + `last_velocity` as point attributes instead of triangulated cloth; `clothMesh` construction is disabled.

## Current state (as found)

*   PBF core runs but has known bugs: `set_neighbors()` in `src/cloth.cpp:296` computes `neighbor_key = hash_box(pos, h)` without applying `i/j/k` offsets (all 27 lookups hit the same cell); `hash_position()` vs `hash_box()` use different schemes; `addGrid()` duplicates `buildGrid()`.
*   `grid.h` (`Field`, `grid`) and `navierStokes.h` (`struct navierStokes`) are empty placeholders — no Eulerian MAC grid / pressure projection yet.
*   `buildClothMesh()` / spring constraints are dead code; `pinned` / `orientation` handling is vestigial.
*   Visuals are debug-level (colored points); no proper fluid shading, no vector-field visualization.

## Revival plan — one step at a time

We will revive incrementally and keep `main` green after each step.

1.  **Step 0 — Baseline & docs (this commit):** Document intent, freeze current behavior, add build/run notes.
2.  **Step 1 — Fix neighbor search:** Correct 27-cell hash lookup, unify hash to `int` key, add unit test for `hash_box`/`set_neighbors`.
3.  **Step 2 — Stabilize PBF loop:** Fix Verlet + `lambda`/`delta_p` denominator, wire `density`/`mass` correctly, re-enable `self_collide` spatial map, tune `h/epsilon/k`.
4.  **Step 3 — Inflow/outflow:** Replace `buildGrid`-every-10-frames hack with a proper emitter + kill-plane, expose `h`, `density`, `simulation_steps` in GUI.
5.  **Step 4 — Viscosity & vorticity:** Re-enable and validate XSPH + vorticity confinement.
6.  **Step 5 — Rendering:** Point sprites / velocity coloring, optional screened Poisson surface or at least blended spheres.
7.  **Step 6 — Eulerian path (optional):** Decide on `grid`/`navierStokes` — either implement a MAC grid pressure solve or remove stubs to keep project purely PBF/SPH.

Contributions should target the current step only; open an issue before jumping ahead.

## cpu-run — cpu-lbm 0a+0b (headless + viewer, still no physics)

Field/grid/tracer core is shared; viewer is a thin GLFW window on top. Still uniform flow `u=(0.05,0)` — LBM lands in Phase 0c.

```bash
cmake -S cpu-lbm -B cpu-lbm/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpu-lbm/build -j
ctest --test-dir cpu-lbm/build --output-on-failure

# headless (Gate 0a)
./cpu-lbm/build/tracers2d_headless --steps 2000
./cpu-lbm/build/tracers2d_headless --nx 256 --ny 128 --particles 10000 --steps 5000 --csv cpu-lbm/out/tracers.csv

# viewer (Gate 0b) — needs GLFW + OpenGL; `brew install glfw` on macOS
./cpu-lbm/build/tracers2d_live
./cpu-lbm/build/tracers2d_live --nx 256 --ny 128 --particles 10000 --scale 3
# big obstacle + collision (SDF) — default slip, big circle at center
./cpu-lbm/build/tracers2d_live --circle 115 64 23
./cpu-lbm/build/tracers2d_live --block 115 64 46 46
./cpu-lbm/build/tracers2d_live --circle 80 64 18 --block 180 64 40 40
./cpu-lbm/build/tracers2d_live --coll bounce   # or slip|stick|passive
./cpu-lbm/build/tracers2d_live --no-obstacle
```

Headless CSV: `step,avg_x,avg_y,recycled,total_recycled` (Gate 0a: `ctest` green, stable count, `total_recycled>0` when `steps > nx/0.05`). Viewer: dark field + warm-white points streaming L→R, warm-grey block/circle obstacle, recycle visible, resize safe. Controls `SPACE` pause, `R` reseed, `+/-` add/remove 100, `C` cycle `slip→stick→bounce→passive`, `B` toggle `circle→block→none`, `ESC`/`Q` quit. Full flags in [cpu-lbm/README.md](cpu-lbm/README.md). Real LBM (`cyl2d_*`, `tunnel3d`) still not built — Phase 0c.

## Build & run (legacy PBF — frozen)

```bash
mkdir build && cd build
cmake .. && make -j
./windsim -f ../scene/pinned2.json   # default if -f omitted
# -r <project_root>  # override shader/texture search path
# -a / -o            # sphere lat/lon subdivs
```

Controls: `Space` pause, mouse drag to orbit, scroll to zoom. `cloth.buildGrid()` injects while running — close/reopen or pause to stop inflow.

## Repo map

```
src/cloth.h,cpp          # fluid solver (misnamed, will be renamed to FluidSystem)
src/pointMass.h          # particle struct
src/grid.h,cpp           # (stub) Eulerian grid
src/navierStokes.h,cpp   # (stub) Navier-Stokes
src/clothSimulator.*     # nanogui viewer + draw calls
src/collision/           # Sphere / Plane
CGL/                     # renderer math lib
ext/nanogui/             # GUI
scene/*.json             # cloth-format scene descs (reused for fluid params)
shaders/                 # Default.vert + *.frag
```

## License

Original assignment skeleton © UC Berkeley CS184. Modifications for WTFS retain the same educational-use terms unless noted.
