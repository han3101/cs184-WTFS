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
