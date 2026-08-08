# WTFS — Three-Project Rewrite Plan

## Context

`cs184-WTFS` began as the Berkeley CS184 cloth assignment and was repurposed into a
"wind tunnel" by replacing the mass-spring solver with Position Based Fluids. It doesn't
work — particles fly at nonsensical speeds — and `docs/findings.md` catalogues why.

The deeper problem is not the bugs. **PBF is a liquid solver.** Its job is enforcing
incompressibility on a particle set with a free surface. Air in a wind tunnel has no free
surface, and what we actually want — wake structure, separation points, drag — is a
property of the *velocity field*, not of any particle. No amount of bug-fixing converts
PBF into a wind tunnel.

The reframe this plan is built on:

> **Particles become visualization, not simulation.** Solve for a velocity field on a
> grid; sprinkle massless tracers that are advected by it.

That gives the intended "wind streaming around a car" visual while making particle count
a pure rendering knob that cannot destabilize anything.

**Solver choice: Lattice Boltzmann (D2Q9 → D3Q19).** Chosen over MAC-grid + pressure
projection because (1) collide and stream are entirely *local* — no global Poisson solve,
which is the hardest thing to make fast on a GPU; (2) obstacles are handled by bounce-back
on a voxel mask, so an arbitrary car OBJ is a voxelization problem rather than a cut-cell
problem; (3) pressure comes free from local density, so drag/lift are a surface sum; and
(4) it is what production automotive aero actually uses.

Note LBM is not an alternative to Navier–Stokes — it *recovers* NS in the low-Mach limit.
There is no later, more-advanced solver stage waiting.

**Target: three projects in one repo.**

| | Project | Role |
|---|---|---|
| 1 | `legacy-pbf/` | The broken PBF sim, frozen. Reference + postmortem. |
| 2 | `cpu-lbm/` | Correct C++ CPU solver, 2D + 3D. The permanent validation oracle. |
| 3 | `gpu-lbm/` | WebGPU port, 2D + 3D. Metal on Apple Silicon, Vulkan/D3D12 on NVIDIA. The real product. |

Projects 2 and 3 deliberately mirror each other's 2D/3D structure: same cases, same
validation targets, two backends. That is what makes the speedup measurable rather than
asserted, and it makes the GPU port a translation exercise rather than a rewrite.

Two implementations of the solver, not three. PyTorch is a **stretch item for
differentiable simulation** — a research direction, not a performance rung.

## Repo structure

```
cs184-WTFS/
├── plan.md                  # this document
├── README.md                # rewritten: index of the three projects
├── docs/
│   ├── findings.md          # moved; v1 postmortem
│   ├── units.md             # lattice ↔ physical conversion cheatsheet
│   ├── validation.md        # reference values + how each is measured
│   └── benchmarks.md        # CPU vs GPU MLUPS, resolution/Re ladder
├── assets/
│   ├── meshes/              # sphere.obj, car.obj, tree.obj
│   └── scenes/              # tunnel configs (JSON)
├── legacy-pbf/              # Project 1 — self-contained, frozen
│   ├── CGL/  ext/nanogui/  src/  shaders/  scene/
│   └── CMakeLists.txt
├── cpu-lbm/                 # Project 2
└── gpu-lbm/                 # Project 3
```

`legacy-pbf/` stays fully self-contained (keeps its own `CGL/` and `ext/nanogui/`).
Projects 2 and 3 pull dependencies via CMake `FetchContent` rather than vendoring — no
shared `ext/` tangle.

---

## Project 1 — `legacy-pbf/` (freeze)

Preserve the broken implementation as a buildable reference. **Do not fix its bugs.**

1. `git mv` into `legacy-pbf/`: `src/`, `CGL/`, `ext/`, `shaders/`, `scene/`, `CMakeLists.txt`.
   Use `git mv` so history and blame survive.
2. Fix paths so it still builds:
   - root `CMakeLists.txt` → `add_subdirectory(ext/nanogui)`, `add_subdirectory(CGL)` and
     `include_directories` are relative and should survive the move intact
   - `src/CMakeLists.txt` — `EXECUTABLE_OUTPUT_PATH ..` and the `install(...)` destination
     reference `WindSim_SOURCE_DIR`; verify both still resolve
   - `src/main.cpp` — the `project_root` default used for shader/texture lookup
     (`-r` override exists as an escape hatch)
3. `git mv findings.md docs/findings.md`.
4. Tag the pre-move state: `git tag v1-pbf`.
5. `legacy-pbf/README.md` — one paragraph: what this is, why it's frozen, pointer to
   `docs/findings.md`.

Verification: `cd legacy-pbf && mkdir build && cd build && cmake .. && make -j && ./windsim -f ../scene/windTest.json`
still runs exactly as before the move.

**Non-goal:** the P0/P1 fixes listed in `findings.md` §3 are now obsolete. Do not spend
time on them.

---

## Project 2 — `cpu-lbm/` (C++17, the oracle)

The solver is a **library with zero graphics dependencies**. Viewers are thin apps on top.
This keeps the oracle pure, portable and CI-testable while still allowing a live 2D window.

```
cpu-lbm/
├── CMakeLists.txt
├── src/                     # pure solver — no graphics deps
│   ├── lattice.h            # D2Q9 + D3Q19 velocity sets, weights, opposite indices
│   ├── units.h/.cpp         # physical ↔ lattice conversion
│   ├── field.h              # SoA storage + flat indexing
│   ├── lbm.h/.cpp           # collide, stream, macroscopic
│   ├── boundary.h/.cpp      # bounce-back, Zou-He inlet, zero-gradient outlet
│   ├── obstacle.h/.cpp      # analytic shapes + voxel mask
│   ├── obj.h/.cpp           # minimal OBJ loader
│   ├── voxelize.h/.cpp      # mesh → solid mask
│   ├── tracers.h/.cpp       # massless tracer particles advected by velocity field
│   ├── probes.h/.cpp        # drag/lift via momentum exchange, wake probe, Strouhal
│   └── io.h/.cpp            # PNG frames (stb_image_write), CSV metrics, raw volume dump
├── apps/
│   ├── tracers2d_headless.cpp # Phase 0a: uniform field + particles, no window
│   ├── tracers2d_live.cpp   # Phase 0b: same + GLFW window (replaces cyl2d_live early)
│   ├── cyl2d_live.cpp       # Phase A3+: LBM-driven version (evolves from tracers2d_live)
│   ├── cyl2d_batch.cpp      # PNG + CSV, CI-friendly
│   ├── cavity2d.cpp         # validation only
│   └── tunnel3d.cpp         # headless
├── viewer/                  # GLFW + GL; linked only by *_live apps
│   └── field_viewer.h/.cpp  # float texture + fullscreen quad + colormap shader + particle overlay
└── tests/
```

Dependencies via `FetchContent`: GLFW (viewer only), `stb_image_write.h`, a unit-test
framework (Catch2 or doctest).

### Execution order for cpu-lbm — scaffolding first, physics last

Builds in three increments. Each is shippable and gates the next. No LBM until the loop
and viewer are proven.

#### Phase 0a — Minimal working simulator (no physics, headless)

Goal: the repo builds, owns a grid, owns a velocity field, and streams particles through
it in a tight loop. No Navier-Stokes — the field is a stub uniform flow `u = (u0, 0)`.

Deliver:

- `cpu-lbm/CMakeLists.txt` — builds `src` as a static lib + `apps/tracers2d_headless` + `tests`. No graphics dep yet.
- `src/field.h` — SoA velocity storage + flat indexing that LBM will reuse verbatim:
  `idx = y*nx + x` (2D), `idx = (z*ny + y)*nx + x` (3D); helpers `idxToXY`, bounds checks.
  Stores `std::vector<float> ux, uy` (and `uz` for 3D) sized `ncells`. Allocate once.
  Intentionally lay out as `f[q*ncells + idx]`-ready — document the future LBM layout in a comment so Phase 0c is a drop-in.
- `src/tracers.h/.cpp` — `struct Tracer { float x, y; }` (3D adds z), `TracerSet` with `initRandom(n, domain)`, `advect(dt)` that samples the field with bilinear (2D) / trilinear (3D) interpolation, does `pos += u * dt`, and recycles at outlet → inlet with random spanwise position. Particle count is a pure visual knob; never touches field.
- `src/units.h` stub — just `constexpr float U_LB = 0.05f` and domain size constants; full `units.h` (Re/τ/dx/dt conversion) lands in Phase 0c.
- `apps/tracers2d_headless.cpp` — constructs `Field(nx=256, ny=128, u0=0.05)`, `TracerSet(10000)`, loops `steps=5000`, prints CSV `step, avg_x, recycled_count` or dumps `out/tracers.csv`. No window, CI-runnable.
- `tests/test_field.cpp` — flat-index round-trip, out-of-bounds clamp, SoA stride; `tests/test_tracers.cpp` — particle seeded at inlet with uniform flow reaches outlet in `nx/u0` steps ± interpolation tolerance, recycling preserves count.

Gate 0a: `cmake .. && make -j && ./tracers2d_headless --steps 2000` exits 0, CSV shows monotonic `avg_x` advance and stable count; `ctest` green. No graphics, no LBM.

Explicit non-goals for 0a: `lattice.h`, `lbm.h`, `boundary.h`, `obstacle.h`, `voxelize.h`, `probes.h`, viewer — none exist yet.

#### Phase 0b — Viewer (see it move)

Goal: turn the headless loop into something you can watch. Same uniform field, now visible.

Deliver:

- `viewer/field_viewer.h/.cpp` — GLFW + GL, linked **only** by `*_live` apps so the solver lib stays headless. Minimal: fullscreen quad sampling a float texture for `|u|` colormap (optional for 0b — solid color is fine), plus `GL_POINTS` particle overlay fed from a dynamic VBO updated each frame from `TracerSet`. Shader pair `shaders/particles.vert/.frag`. Keep it <300 LOC.
- `apps/tracers2d_live.cpp` — copies the headless loop but drives `FieldViewer::beginFrame/drawField/drawTracers/endFrame` at 60 Hz. Keyboard: `SPACE` pause, `R` reseed, `+/-` add/remove tracers (proves count is decoupled).
- `CMakeLists.txt` update — `FetchContent` GLFW, `find_package(OpenGL)`, `add_executable(tracers2d_live ...)` linking `viewer`.

Gate 0b: `./tracers2d_live` opens a window, particles stream left→right at constant speed, recycle visibly at inlet, frame time <16 ms at 10k particles on integrated graphics. Resizing window does not crash (field resizes or letterboxes — pick one and document).

Explicit non-goal: no LBM, no obstacles, no vorticity viz — field is still uniform. If colormap is stubbed, document it as TODO for Phase D.

#### Phase 0c — Plug in physics (LBM replaces the stub)

Goal: keep every file from 0a/0b, replace `Field`'s uniform fill with a real LBM step. Tracers don't change — they just sample a now-non-uniform `u`.

Deliver (this is where the original Project 2 file list arrives):

- `src/lattice.h` — D2Q9 + D3Q19 velocity sets, weights, opposite indices, `cs² = 1/3`.
- `src/units.h/.cpp` — full physical ↔ lattice conversion: pick `u_lb = 0.05` (Mach ≈ 0.087), `ν_lb = u_lb·L_lb/Re`, `τ = 3ν_lb + 0.5` with `τ > 0.51` assert, `dx = L_phys/L_lb`, `dt = dx·u_lb/u_phys`. Documented in `docs/units.md`.
- `src/field.h` upgrade — add `std::vector<float> f, f_next` sized `Q*ncells` in SoA `f[q*ncells + idx]` ping-pong layout; `ux/uy/uz` become derived views computed by macroscopic step. Keep the same indexing helpers from 0a.
- `src/lbm.h/.cpp` — `macroscopic()`, `equilibrium()`, `collideBGK()`, `streamPull()` (gather: `f_new[q][idx] = f_old[q][idx - c_q]`). Pull, not push — no atomics, direct GPU mapping.
- `src/boundary.h/.cpp` — bounce-back on solid mask, Zou-He velocity inlet, zero-gradient outlet.
- `src/obstacle.h/.cpp` + `src/voxelize.h/.cpp` + `src/obj.h/.cpp` — analytic cylinder/sphere + `mesh → solid mask` for car OBJ.
- `src/probes.h/.cpp` + `src/io.h/.cpp` — drag/lift via momentum exchange, wake probe + Strouhal FFT, PNG/CSV dump.
- `apps/cyl2d_live.cpp` — evolves from `tracers2d_live.cpp`: same viewer, same tracer code, but field is now stepped by `lbm.step()` each frame. Keep `tracers2d_live.cpp` around as a regression stub or fold it in behind a `--uniform` flag.

Gate 0c: `./cyl2d_batch --re 100 --steps 20000` reproduces the Phase A3 targets (St 0.164–0.167, Cd 1.32–1.36) — i.e. 0c *is* Phase A3 done, with the scaffolding already proven. `./tracers2d_live --uniform` still passes Gate 0b (no regression).

After 0c, continue with the validation ladder as originally planned (A1→A2→A3 is now mostly satisfied by 0c; run A1/A2 explicitly as regression, then B1/B2 for 3D).

### Core algorithm (lands in Phase 0c)

Velocity sets (`lattice.h`), with `cs² = 1/3`:

- **D2Q9** — `w = {4/9, 1/9 ×4 (axis), 1/36 ×4 (diag)}`
- **D3Q19** — `w = {1/3 (rest), 1/18 ×6 (face), 1/36 ×12 (edge)}`

Per step:

1. **Macroscopic** — `ρ = Σ fᵢ`, `u = (1/ρ) Σ fᵢ cᵢ`
2. **Equilibrium** — `fᵢᵉ𝑞 = wᵢ ρ [1 + 3(cᵢ·u) + 4.5(cᵢ·u)² − 1.5(u·u)]`
3. **Collide (BGK)** — `fᵢ* = fᵢ − (1/τ)(fᵢ − fᵢᵉ𝑞)`
4. **Stream (pull)** — `fᵢⁿᵉʷ(x) = fᵢ*(x − cᵢ)`
5. **Boundaries** — bounce-back on solid, Zou-He at inlet, zero-gradient at outlet

Two decisions that exist purely to make the Project 3 port mechanical:

- **SoA layout, flat index `f[q * ncells + idx]`**, ping-pong between two buffers. Use the
  *identical* layout in WGSL later. Phase 0a establishes the helpers; 0c fills the buffers.
- **Pull streaming (gather), not push (scatter)** — reads neighbours, writes own cell. Maps
  directly to one GPU invocation per cell with no atomics.

### Unit system (`docs/units.md`) — lands in Phase 0c

The most common source of silent wrongness. Write it down and test it:

- pick `u_lb = 0.05` (Mach ≈ 0.087 — must stay well below `cs ≈ 0.577`)
- `ν_lb = u_lb · L_lb / Re`
- `τ = 3ν_lb + 0.5`, require `τ > 0.5` (practically `> 0.51`; BGK destabilises as `τ → 0.5`
  — that ceiling is what forces Phase E)
- `dx = L_phys / L_lb`, `dt = dx · u_lb / u_phys`

Phase 0a uses a hardcoded `u_lb` with no conversion; the full system is tested in 0c with
`tests/test_units.cpp` (round-trip physical ↔ lattice).

### Phase A — 2D validation (D2Q9) — starts after 0c

Gates, cheapest first, each blocking:

| # | Case | Target |
|---|---|---|
| A1 | Poiseuille channel | parabolic profile vs analytic, L2 error < 1e-3 |
| A2 | Lid-driven cavity, Re=1000 | centreline u/v vs Ghia et al. 1982 |
| A3 | Cylinder, Re=100 | St = fD/U ≈ 0.164–0.167; Cd ≈ 1.32–1.36 |

A1 first because it catches unit-system and boundary-condition errors with almost no code.
A3 is the payoff — visible Kármán vortex street. Measure St by FFT or zero-crossing count
of transverse velocity at a fixed wake probe. If 0c was validated against A3 directly, A1/A2 are regression checks here.

### Phase B — 3D (D3Q19)

Mostly a velocity-set and indexing change on top of Phase A, plus geometry.

| # | Case | Target |
|---|---|---|
| B1 | Sphere, Re≈10⁴ | Cd ≈ 0.47 (±10% acceptable at this resolution) |
| B2 | Voxelized `sphere.obj` | solid mask matches analytic sphere mask |

Add inlet/outlet/freestream BCs and the OBJ voxelizer here. Expect a few steps/sec at 128³
single-threaded — that is fine and expected. `-fopenmp` is already in the toolchain flags
if you want a cheap multiplier.

**This project is the permanent oracle. Keep it in the repo forever.** It is what makes
Phase C tractable instead of a nightmare of "is this a physics bug or a bind-group bug?"

---

## Project 3 — `gpu-lbm/` (WebGPU)

Backend: **Dawn** via `FetchContent` (C++-native, matches the codebase). If Dawn's build
proves painful, fall back to **wgpu-native** (prebuilt binaries, C header). Time-box this
as a spike before committing.

**Mirrors Project 2's 2D → 3D structure.** Porting D2Q9 first means learning WebGPU
against a case that is already validated, small enough to sidestep every buffer-limit
issue, and cheap to diff against the CPU oracle. It also yields the cleanest possible
speedup story: *identical algorithm, identical resolution, identical validation case,
two backends.* The same scaffolding-first order applies: port the uniform-field tracer loop before the LBM kernels.

```
gpu-lbm/
├── CMakeLists.txt
├── shaders/
│   ├── collide_stream_2d.wgsl  # D2Q9 — first target
│   ├── collide_stream_3d.wgsl  # D3Q19
│   ├── boundary.wgsl           # bounce-back + inlet/outlet
│   ├── macroscopic.wgsl        # ρ, u for viz + probes
│   ├── tracers.wgsl            # advect particles, recycle at inlet
│   └── render_*.wgsl
├── src/
│   ├── gpu_context.cpp         # instance / adapter / device / queue
│   ├── lbm_gpu_2d.cpp          # buffers, bind groups, dispatch
│   ├── lbm_gpu_3d.cpp
│   ├── tracers.cpp
│   ├── renderer.cpp
│   └── bench.cpp               # MLUPS harness, shared with cpu-lbm
└── apps/
    ├── cyl2d_gpu.cpp           # same case as cpu-lbm/apps/cyl2d_*
    └── tunnel3d_gpu.cpp
```

### Phase C1 — 2D port (D2Q9)

The WebGPU learning phase. All the API plumbing — adapter, device, buffers, bind groups,
pipelines, dispatch, ping-pong — against a solver you already know is correct.

Gate: reproduces A3 (cylinder at Re=100) with St and Cd matching the CPU run within 1%,
and macroscopic fields agreeing to ~1e-5 relative.

Deliverable beyond correctness: crank 2D resolution past what the CPU can do interactively
(the CPU is real-time around 512×128; the GPU should hold real-time into the multi-megacell
range) and record where each one falls off.

### Phase C2 — 3D port (D3Q19)

Structurally the same port, but now memory limits are real.

**Buffer-limit constraint — decide before writing the 3D WGSL:**

- `maxStorageBufferBindingSize` defaults to **128 MiB**, `maxBufferSize` to **256 MiB**
- D3Q19 fp32 at 256×128×128 = 4.19M cells → **319 MB per buffer**, ×2 for ping-pong
- `maxStorageBuffersPerShaderStage` defaults to **8** (often 10), so 19 per-direction
  buffers is not an option

Resolution: request raised limits at device creation and verify adapter support; fall back
to splitting `f` into ~4 buffers of ~5 directions each. **Start at 128³** (2.1M cells,
160 MB/buffer) to stay comfortably inside limits while the port is being debugged.
None of this applies in C1 — 2D buffers are trivially small.

Workgroup size: start at 64 with a linear index; tune later.

Gate: reproduces B1 (sphere, Cd ≈ 0.47) matching the CPU run within 1%.

### Benchmarking — the contrast deliverable

`bench.cpp` reports **MLUPS** (million lattice updates/sec = `cells × steps / seconds / 1e6`),
the standard LBM performance metric, from both projects on identical cases. Record in
`docs/benchmarks.md`:

| Case | cpu-lbm | gpu-lbm |
|---|---|---|
| 2D cylinder, 512×128 | baseline | speedup ×? |
| 2D cylinder, 4096×1024 | (likely impractical) | still real-time? |
| 3D sphere, 128³ | baseline | speedup ×? |
| 3D sphere, 256×128×128 | (likely impractical) | interactive? |

Rough expectation: single-threaded CPU lands in the tens of MLUPS, a modern discrete or
Apple-silicon GPU in the high hundreds to low thousands — call it one to two orders of
magnitude, and measure rather than assume.

The more interesting framing is not raw speedup but **what becomes possible**: resolution
you can actually afford, the Reynolds number that resolution lets you resolve, and runs
reaching statistical steady state in seconds rather than hours. Log that as a
resolution/Re ladder alongside the MLUPS table.

### Phase D — rendering

Cheap, because the field already exists and WebGPU handles compute and render together
with no readback:

- tracer particles advected by the velocity field — **this is the "wind particles" visual** (already proven in Phase 0b/C1)
- vorticity magnitude and/or Q-criterion isosurfaces for wake structure
- live drag/lift readout

### Phase E — high Reynolds

Add a Smagorinsky LES model, or swap BGK for a TRT/MRT/cumulant collision operator.
Gate: stable at a Reynolds number where BGK diverges.

**Expectation calibration:** a real car at 30 m/s is Re ≈ 8×10⁶; production aero uses
10⁸–10⁹ cells against our ~10⁶. Expect convincing, qualitatively correct wakes and useful
*relative* comparisons (spoiler vs no spoiler). Do not present absolute drag coefficients
as validated.

---

## Stretch / wishlist — differentiable simulation (PyTorch)

A separate research track, not a dependency of anything above. Start only once Phase C is
green.

- Reimplement D2Q9 in PyTorch (streaming is `torch.roll`, collision is elementwise); runs
  on CUDA and Apple MPS from one file
- **Main obstacle:** naive backprop stores every timestep — O(steps × cells × Q). Requires
  gradient checkpointing or an adjoint formulation. Realistic scope is differentiating
  through a short window (50–200 steps), not a full run to steady state
- Applications: obstacle shape optimisation for minimum drag; neural surrogate trained on
  Project 3 output; learned turbulence closure

Worth being explicit that this teaches the *array-programming* slice of PyTorch, not the
ML stack (autograd usage aside — no `nn.Module`, optimisers, training loops, or data
loading). It is not a substitute for learning ML directly.

---

## Non-goals

- Fixing the PBF bugs in `legacy-pbf/` — obsolete, the code is frozen
- SPH — same Lagrangian family as PBF, wrong tool for external aero. Worth learning as a
  separate water-sim side project, not a stage of this one
- Spring/tree fluid-structure coupling — explicitly cut from scope
- OpenGL compute — macOS caps OpenGL at 4.1 and compute shaders need 4.3, so there is no
  path from the legacy stack to Apple Silicon GPU compute
- Absolute-accuracy CFD claims

## Verification

| Stage | Command | Pass condition |
|---|---|---|
| P1 frozen | `cd legacy-pbf/build && cmake .. && make -j && ./windsim -f ../scene/windTest.json` | runs as before the move |
| 0a headless | `./tracers2d_headless --steps 2000` | CSV monotonic avg_x, count stable; `ctest` green |
| 0b live | `./tracers2d_live` | window streams particles L→R at u0, recycle visible, 60fps @10k |
| 0c/A3 | `./cyl2d_batch --re 100 --steps 20000` | St ∈ [0.164, 0.167], Cd ∈ [1.32, 1.36] |
| A1 | `./cavity2d --case poiseuille` | L2 error vs analytic < 1e-3 |
| A2 | `./cavity2d --re 1000` | centreline profile matches Ghia et al. |
| A3 (live) | `./cyl2d_live --re 100` | visible shedding, interactive Re |
| B1 | `./tunnel3d --obstacle sphere --re 1e4` | Cd ≈ 0.47 ± 10% |
| B2 | `./tunnel3d --obstacle assets/meshes/sphere.obj` | mask matches analytic |
| C1 | `./cyl2d_gpu --re 100 --compare ../cpu-lbm/out/cyl2d.raw` | fields agree ~1e-5; St/Cd within 1% |
| C2 | `./tunnel3d_gpu --obstacle sphere --compare ../cpu-lbm/out/sphere.raw` | fields agree ~1e-5; Cd within 1% |
| bench | `./bench --all` in both projects | MLUPS table populated in `docs/benchmarks.md` |
| unit | `ctest` in `cpu-lbm/build` | lattice invariants (Σw=1, Σw·c=0), unit round-trip, voxelizer, tracer advection, field indexing |

Record every measured value in `docs/validation.md` alongside its literature reference, so
regressions are detectable later.

## Suggested order

1. Project 1 freeze + repo restructure (`docs/`, `assets/`, `legacy-pbf/`)
2. **Phase 0a — cpu-lbm scaffolding, no physics** — `field.h` + `tracers.h` + `tracers2d_headless` (headless loop, CI-testable)
3. **Phase 0b — viewer** — `viewer/field_viewer.h` + `tracers2d_live` (GLFW window, particles streaming on uniform field)
4. **Phase 0c — physics** — `lattice.h`/`units.h`/`lbm.h`/`boundary.h`/`obstacle.h` replace uniform stub; `cyl2d_live` now LBM-driven; validate as A3
5. Phase A1 → A2 regression (Poiseuille, cavity — should already pass if 0c did)
6. Phase B1 → B2 (3D + geometry)
7. Phase C1 (2D WebGPU port — learn the API against a known-correct case) + first CPU/GPU benchmark numbers — same 0a→0b→physics split applies on GPU
8. Phase C2 (3D WebGPU port, validated against B)
9. Phase D (rendering — vorticity/Q-criterion, drag readout; tracers already done in 0b)
10. Phase E (high Re)
11. Stretch: differentiable simulation

Both projects keep 2D and 3D paths for their whole life. 2D stays the fast iteration
loop and the benchmark control; 3D is the product. Phase 0's split (field/grid → viewer → physics) is the template for both backends.
