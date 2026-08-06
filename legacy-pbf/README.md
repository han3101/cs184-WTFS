# legacy-pbf — frozen PBF reference

This is the original `cs184-WTFS` Position Based Fluids wind-tunnel attempt, frozen as a buildable reference. It is intentionally not fixed — see `../findings.md` (and planned `../docs/findings.md`) for the P0/P1 bug catalogue and why PBF is the wrong model for external aero.

Build: `mkdir build && cd build && cmake .. && make -j && ./windsim -f ../scene/windTest.json`

Do not edit here; current work continues in `../cpu-lbm/` (CPU LBM oracle) and `../gpu-lbm/` (WebGPU).
