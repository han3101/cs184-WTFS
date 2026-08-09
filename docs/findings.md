# Findings — WTFS Revival

Date: 2026-08-06
Scope: `src/cloth.*`, `src/pointMass.h`, `src/grid.*`, `src/navierStokes.*`, `src/clothSimulator.*`, `src/main.cpp`

## 1. What the project is actually doing today

Wind tunnel in name only. On each `drawContents()` (`src/clothSimulator.cpp:248`) it steps `cloth->simulate()` `simulation_steps=30` times at `frames_per_sec=90` (`dt≈3.7e-4`), with `external_accelerations={gravity=(0,0,4.5)}`. `main.cpp:500` injects a whole `buildGrid()` slab every 10 frames while unpaused. Particles are rendered as `GL_POINTS` with velocity attribute (`drawNormals`/`drawPhong`). There is no wind field, no inlet/outlet, only `+Z` gravity and a `x/y/z>5` cull in `Cloth::simulate():218`.

User observation confirmed: "a lot of particles flying at weird speeds."

## 2. Particle-particle interaction — does it exist?

Yes, but minimal and broken. Intended Position Based Fluids (Macklin & Müller 2013) path in `src/cloth.cpp:119-178`:

```
Verlet -> build_spatial_map() -> set_neighbors() -> 2×(calculate_lambda -> calculate_delta_p -> pos+=delta_p) -> collide()
```

* Kernels `poly6`/`spiky` in `src/cloth.h:91-105`
* Params `h=0.12, rho0=density, eps(relaxation)=160000, k=1e-5, n=4, dq=0.2h` in `src/cloth.h:61-66`

Disabled but present: `viscosity()`/`calculate_omega()`/`vorticity()` (`src/cloth.cpp:369-407`, commented out `simulate():180-190`) and `self_collide()` (`simulate():194`).

No spring/mesh forces remain; `grid`/`navierStokes` are stubs.

## 3. Bugs — ranked by impact

### P0 — `set_neighbors` only checks own cell [FIXING]
`src/cloth.cpp:296-317`
```cpp
for i/j/k in -1..1:
  neighbor_key = hash_box(pos, h) // BUG: ignores i,j,k
  if map.count(neighbor_key) ...
```
27 lookups hit the same cell. Effective neighbor count ~1/27, `lambda`/`delta_p` severely underestimated → particles don't enforce incompressibility → speeds dominated by unchecked Verlet gravity.

Fix: `hash_box(pos + Vector3D(i*h, j*h, k*h), h)` and dedup. Also `hash_box` uses bit-mask `&0x3FF` so negative cells alias; consider offset or use `hash_position` scheme consistently.

### P0 — Hash scheme split
`hash_position()` (`src/cloth.cpp:268`, `float` key `(x*31+y)*31+z` with cell size `w=3*width/(num_w-1)`) vs `hash_box()` (`int` bit-packed). `build_spatial_map()` uses `hash_box`, `self_collide()`/`hash_position()` path uses the other. Inconsistent.

Fix: unify on `hash_box` int key; remove or adapt `hash_position`/`map<float,...>`.

### P1 — `map` type mismatch
`src/cloth.h:122` `unordered_map<float, vector<PointMass*>*> map;` but `hash_box` returns `int`. Float key causes hashing/aliasing issues and the `float` hash in `hash_position` is lossy.

Fix: `unordered_map<int, vector<PointMass*>*>`.

### P1 — `viscosity`/`vorticity` disabled
Implemented but never called. Without XSPH, particle velocities stay noisy even after neighbor fix.

Fix: re-enable after P0, gated by param.

### P1 — Mass/density mismatch
`mass = width*height*density / num_w/num_h` (`simulate():122`) is cloth areal mass, not `rho0` volumetric density for PBF. `calculate_lambda` takes `density` as `rho0` while also scaling `p_i *= mass`. Coupling is ad-hoc.

Fix: expose `rest_density` and `particle_mass` separately; pass consistently.

### P2 — Double grid builder
`buildGrid()` and `addGrid()` in `src/cloth.cpp:45,83` are duplicates. `main.cpp:500` keeps appending without clearing; `reset()` just `clear()`s vector but leaks `neighbors` allocs and doesn't reset `last_position`.

Fix: single `emitSlab()` + proper `reset()` + emitter class.

### P2 — Memory leak / per-particle alloc
`PointMass::neighbors = new vector<PointMass*>` per particle (`src/pointMass.h:19`), never freed except `reset()` `clear()` (which leaks). `build_spatial_map()` `new vector<PointMass*>` per cell, deleted each rebuild — okay but `map.clear()` after delete loop is fragile.

Fix: `std::vector<PointMass*> neighbors;` by value, or `unique_ptr`.

### P2 — `self_collide` scaling
`pm.position += correction / count / simulation_steps` (`src/cloth.cpp:264`) — `simulation_steps` should not scale collision correction; also thickness `*1` is magic.

### P3 — `poly6`/`spiky` edge cases
`spiky` divides by `r.norm()` without `EPS` guard except `>h` early out; `r=0` (self) already filtered but near-zero still unstable.

### P3 — `grid`/`navierStokes` stubs
`src/grid.h` has empty `Field`/`grid`, `src/navierStokes.h` empty `struct navierStokes{}`. Either implement MAC grid or delete to avoid confusion. Current CMake still compiles them.

## 4. What is *not* implemented

* No real wind: no uniform field, no boundary layer, no inlet velocity profile.
* No pressure projection / divergence-free velocity.
* No inlet/outlet BC, no kill plane.
* No surface reconstruction / proper fluid shading.

## 5. Plan — updates will be tracked here

- [x] Step 0 — Baseline docs (`README.md`, this file)
- [x] Step 1 — Fix `set_neighbors` + unify hash to `int` (P0) — fixed `hash_box(pos+offset)` and dedup (src/cloth.cpp:296), `map<float>`->`map<int>` (src/cloth.h:122, src/cloth.cpp:236), `self_collide` now uses `hash_box` (src/cloth.cpp:245), `spiky_kernel` guards `r<1e-9` (src/cloth.h:98)
- [x] Step 1b — Fix `map<float>` -> `map<int>` type — done with Step 1
- [ ] Step 2 — Stabilize PBF: wire `rest_density`/`mass`, validate `lambda` denom, re-enable `self_collide`
- [ ] Step 3 — Emitter/kill-plane, remove `buildGrid` hack, expose params in GUI
- [ ] Step 4 — Re-enable `viscosity`/`vorticity` with toggles
- [ ] Step 5 — Rendering pass (point sprites, velocity color)
- [ ] Step 6 — Decide `grid`/`navierStokes` fate (implement or remove)

Each step: edit → note in §6 → keep `windsim` buildable.

## 6. Changelog

* 2026-08-06 — Created findings, ranked bugs, started P0 neighbor fix.
* 2026-08-06 — Fixed P0: `set_neighbors` now scans 27 cells correctly, `map` unified to `int`, `spiky` guarded.

---

# Part II — `cpu-lbm` Phase 0c findings

Everything above is the frozen v1 PBF postmortem and its P0/P1 items are obsolete
(see `plan.md` § Non-goals). This part tracks the C++ LBM oracle instead.

Date: 2026-08-09
Scope: `cpu-lbm/src/{lattice.h,lbm.*,boundary.*,probes.*,field.*,units.h}`, `cpu-lbm/apps/cyl2d_*`

## 7. Phase 0c — bugs found and fixed

The first 0c implementation built cleanly, passed all six test suites, and exited
`0` — while diverging to NaN around step 500 (`maxu` 0.10 → 224 → 5029 → NaN) and
reporting `Cd=-nan`, `St=0.40`. Four defects:

### F1 — bounce-back reflected nothing [FIXED]
`lbm.cpp` (streamPull) + `boundary.cpp` (applyBounceBack, now deleted)

`streamPull` left solid-sourced directions holding the cell's own collided value
and deferred to `applyBounceBack`, which read `f_next` at *solid* cells. Solid
cells were never collided or streamed into, so `f_next` there held the initial
equilibrium at `u0` forever. The obstacle acted as a constant freestream source
rather than a no-slip wall — this was the divergence.

Fix: halfway bounce-back fused into the pull step —
`f[q][x] = f_collided[opp(q)][x]` whenever the pull source is solid. One branch,
no separate pass, and it maps to a single WGSL invocation in Phase C1.

### F2 — no boundary condition on the north/south walls [FIXED]
`lbm.cpp` (streamPull)

`boundary::applyAll` covered only the west inlet and east outlet. At `y=0` and
`y=ny-1` the out-of-domain pull source fell through to "keep my own collided
value" — not a wall, not periodic, not free-slip. The F1 fix covers this for
free: out-of-domain is treated identically to solid, giving no-slip channel
walls, which is what the Re=100 cylinder case wants.

### F3 — drag had the wrong sign convention [FIXED]
`probes.cpp` (computeDragLift)

Used Ladd momentum exchange as `(f_q − f_opp)·c_q`. The outgoing population
carries `+c_q` into the wall and removing the bounced population also transfers
`+c_q`, so the terms **add**: `(f_q + f_opp)·c_q`. As written it was a small
difference of near-equal numbers, so Cd was noise regardless of the flow.

### F4 — Strouhal was 20× high [FIXED]
`probes.*` + `apps/cyl2d_batch.cpp`

The wake probe was sampled inside `if(step%20==0)` but `estimateFrequency`
defaulted to `dt=1`. `WakeProbe` now stores its own sample interval via
`init(x, y, sampleEvery)` so the cadence cannot drift from the caller's loop.
Frequency estimation also discards the first half of the history — counting the
quiescent startup transient biased the frequency low.

### F5 — Zou-He inlet omitted the transverse term [FIXED]
`boundary.cpp` (applyZouHeInlet)

`f5`/`f8` were set without the `±½(f4−f2)` correction, so the inlet leaked
`uy = (f2−f4)/ρ` instead of enforcing `uy=0` as its comment claimed.

### F6 — the tests could not fail [FIXED]
`tests/test_lbm.cpp`, `tests/test_boundary.cpp`

All six suites passed against the diverging solver. `test_lbm` only checked that
uniform equilibrium flow stays uniform — true for *any* streaming scheme,
including the broken one, and no LBM test contained an obstacle. The Zou-He test
read back `fld.ux`, which `applyZouHeInlet` had just written by hand; it never
recomputed `u` from `f`. The "bounce conserves mass" test permuted a field whose
populations were all identical.

Fix: the boundary inlet test now perturbs `f2`/`f4` and recomputes macroscopics
from `f`; `test_lbm` gained a closed-box mass/decay test and a Poiseuille profile
check (Gate A1). The dead cached `rho/ux/uy` writes in the inlet/outlet were
removed so nothing can be self-verified that way again.

## 8. Phase 0c — open items

### O1 — closed-box decay test fails on threshold, not physics [OPEN]
`tests/test_lbm.cpp`

```
PASS closed box conserves mass
FAIL closed box decays to rest (no-slip walls)
FAIL closed box sheds x-momentum into walls
```

Mass conservation passing while decay fails points at the thresholds, not the
bounce-back. The box is 24×24 at `tau=0.8` (`nu=0.1`), so one viscous decay time
is `L²/(nu·π²) ≈ 583` steps and the test only runs 400. Raise to ~3000 steps,
then set `umax`/momentum bounds from the observed decay rather than guessed
constants. Confirm the decay is exponential with the expected time constant —
that turns a threshold check into an actual physics check.

### O2 — Poiseuille profile is 4× outside the A1 target [OPEN]
`tests/test_lbm.cpp`

```
poiseuille: umax=0.06388 relative L2=3.95e-03   (target < 1e-3)
```

Two things to separate before touching the threshold:

1. **`umax` is 15% low.** Fully-developed flow at `u0=0.05` should give
   `umax = 1.5·u0 = 0.075`. Either 12000 steps is short of steady state (the
   development time is `H²/nu ≈ 4410` steps, so ~2.7 time constants) or flow
   rate is being lost. Run a convergence sweep and check `∫u dy` at the inlet,
   mid-channel, and outlet columns — if it decays with `x`, the zero-gradient
   outlet is the leak.
2. **The 3.95e-3 shape error.** Plausibly O(Ma²) compressibility: this channel
   is pressure-driven, `Ma ≈ 0.087`, and `Ma² ≈ 7.6e-3` is the right order.
   Test by halving `u0` to 0.02 — if the error drops ~6× the diagnosis holds,
   and the fix is either running A1 at lower `u_lb` or documenting `< 5e-3` as
   the achievable target at `u_lb=0.05`. If the error does *not* scale with
   `u0²`, it is a boundary-condition defect and needs real investigation.

Do **not** simply relax the threshold until (1) and (2) are distinguished.

### O6 — cyl2d_live explodes on startup, particles freeze [OPEN]
`apps/cyl2d_live.cpp` + `src/lbm.cpp` / `src/boundary.cpp` / `src/field.cpp` / `apps/live_common.h`

**Observed:** `cyl2d_live` (LBM D2Q9, default `400×80, Re=100, u0=0.05, D≈20`) diverges immediately after launch — velocity field spikes / NaN within first seconds — and tracer particles stop advecting (frozen / sampled as zero or NaN). `tracers2d_live --uniform` and `cyl2d_live --uniform` remain stable, so the shared `FieldViewer` / `live_common.h` framework is not the cause; the LBM step or its coupling to the live loop is. No separate issue was filed before this TODO.

**Hypotheses to separate (do not fix blindly):**
1. **Tau / units mismatch in live loop:** `live_common.h` `LiveParams.dt` (particle `dt`) vs LBM `tau` vs physical `dt` confusion; `updatePhysics()` recomputes `tau` from `Re/D/u0` but `cyl2d_live` default `D` derived from `ny` vs obstacle `r` may mismatch `cyl2d_batch` proven path.
2. **Solid mask rebuild race:** `onObstacleChange` rebuilds `solid` mid-run without re-initializing `f`/`f_next` at newly-solid / newly-fluid cells → stale distributions in `field.f` fed to `collideBGK`/`macroscopic` → `rho→0` → `u→Inf/NaN`.
3. **Bounce-back double-reflect:** F1 fix fused bounce into `streamPull`; `boundary::applyBounceBack` was deleted but `live_common.h` obstacle cycling still calls `updatePhysics()` → `buildSolidMask` → next `streamPull` may double-apply if any legacy boundary pass remains.
4. **Inlet/outlet BC missing vs `--uniform`:** `cyl2d_live --uniform` bypasses boundaries; with LBM, `applyZouHeInlet`/`applyOutlet` (or fused wall) not applied at startup before first `macroscopic()` → `rho/u` inconsistent → tracers sample NaN.
5. **Particle advect on NaN field:** `tracers.advect(field, dt, obstacles, Passive)` bilinearly samples `ux/uy`; if any `ux` is NaN, all downstream positions become NaN and `recycled` logic drops particles.

**Investigation plan (TODO):**
- [ ] Reproduce headless: `./cyl2d_batch --re 100 --steps 500 --nx 400 --ny 80` with `printf max|u|, min rho, max f` every 10 steps; confirm headless diverges or not — isolates viewer vs solver.
- [ ] Reproduce live with `--uniform` vs LBM at same `nx/ny/u0/D`; log `tau`, `maxu`, `min rho` in title bar or `stderr` before divergence.
- [ ] Add `field.isFinite()` guard (scan `f/rho/ux/uy` for `isfinite`) assert after `macroscopic()`; break and dump `field` raw on first failure.
- [ ] Compare `cyl2d_live` startup sequence to `cyl2d_batch`: `initEquilibrium` → `buildSolidMask` → `collideAndStream` → `applyAll` → `macroscopic` ordering and `dt` scaling.
- [ ] Do not touch `live_common.h` particle emission until field is proven finite; fix field first, then re-test particles.

**Exit criteria:** `cyl2d_live --re 100` runs ≥2000 steps with `max|u| < 2·u0` and `rho ∈ [0.9,1.1]`, particles visibly advect and recycle; same `Re/D/u0` as `cyl2d_batch` passes Gate A3 narrow check.

### O3 — Gate A3 unmeasured [OPEN]
`apps/cyl2d_batch.cpp`

`cyl2d_batch --re 100 --steps 20000` has not been run since the F1–F5 fixes, so
St and Cd are unknown. The solver is stable (verified separately: `maxu ≈ 1.75·u0`
through 2000 steps, which is right for flow past a cylinder), but stable is not
validated. Expect Cd to run high: D=20 cells is coarse, halfway bounce-back
staircases the surface, and the cylinder blocks a quarter of an 80-cell channel,
which raises Cd relative to the unbounded literature value. First knobs are
`--ny 160` (D→40) and a longer domain, not the physics.

### O4 — no PNG output [OPEN]
`io.cpp` — `writePNG` is a stub returning `false` and nothing calls it. Plan
lists PNG frames under 0c; deferred to Phase D with the rest of rendering.

### O5 — voxelizer and OBJ loader absent [OPEN]
`voxelize.*` and `obj.*` are on the plan's 0c deliverable list but were not
written. Nothing in 2D needs them; they belong with the Phase B 3D path.

## 9. Changelog — Part II

* 2026-08-09 — Added O6 TODO: cyl2d_live explodes on startup, particles freeze (live-only divergence, hypotheses + repro plan).
* 2026-08-09 — Reviewed Phase 0c. Found F1–F6, fixed all six, logged O1–O5.
