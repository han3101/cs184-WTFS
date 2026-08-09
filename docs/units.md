# Unit system — lattice ↔ physical conversion

Source: `plan.md` § Unit system, `cpu-lbm/src/units.h`.

## Relations

```
u_lb = 0.05          Mach = u_lb / cs ≈ 0.087  (must stay << 0.577)
nu_lb = u_lb * L_lb / Re
tau   = 3*nu_lb + 0.5   require tau > 0.51  (BGK stability, tau→0.5 diverges)
dx = L_phys / L_lb
dt = dx * u_lb / u_phys
cs2 = 1/3
```

`L_lb` is characteristic length in lattice cells (e.g. cylinder diameter D in cells).
`L_phys`, `u_phys` are the physical scales — if unknown, leave `dx=dt=1` and work purely in lattice units.

## Examples

| Case | Re | D (L_lb) | u_lb | nu_lb | tau |
|------|----|----------|------|-------|-----|
| Cylinder Re=100 | 100 | 20 | 0.05 | 0.010 | 0.53 |
| Cylinder Re=150 | 150 | 20 | 0.05 | 0.0067 | 0.520 |
| Cavity lid 1000 | 1000 | 128 | 0.05 | 0.0064 | 0.519 |
| Higher Re needs larger D | 1000 | 40 | 0.05 | 0.002 | 0.506 → unstable, need larger D or higher u_lb (but keep Mach<0.15) |

## Usage

```cpp
auto units = LbUnits::fromRe(100, 20, 0.05);
assert(units.stable());
float tau = units.tau; // 0.53
// or direct:
float tau2 = tauFromRe(100, 20);
```

Round-trip: `physical length = lattice length * dx`, `physical time = steps * dt`.

## Stability ceiling

BGK with `tau → 0.5` amplifies round-off. Require `tau > 0.51` in code (`assert`). If your target Re gives `tau < 0.51`, increase `L_lb` (resolution) or use MRT/TRT (Phase E), not smaller `u_lb` alone.
