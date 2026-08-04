# libphyz 0.1

`libphyz` is PhyzBox's frontend-independent C++20 astrodynamics library. It owns the authoritative double-precision state; native Win32/OpenGL and Godot are clients of the same algorithms.

## Included algorithms

- Stable `BodyId`, explicit unit-system metadata, body state/metadata separation.
- Composable force pipeline with Newtonian, Plummer-softened and Paczynski-Wiita pair gravity plus constant acceleration.
- Fixed-step Leapfrog-2 and Yoshida-4 symplectic integrators with compatibility traits and step reports.
- Mechanical energy, linear/angular momentum, center of mass and momentum-residual diagnostics.
- Cartesian-to-osculating orbital elements for elliptic, parabolic and hyperbolic conics.
- Collision, close-approach, event-horizon and Roche-limit detectors using within-step threshold interpolation.
- Branch trajectory prediction with time-tagged impulse maneuvers. The source simulation is never mutated.
- Versioned deterministic text snapshots, including units, forces, integrator and state.

## Build and test

```powershell
cmake -S . -B build -DPHYZ_BUILD_NATIVE_APP=OFF -DPHYZ_BUILD_TESTS=ON
cmake --build build
cmake --build build --target test
```

The install target exports `phyz::libphyz` and `libphyzConfig.cmake` for downstream CMake projects.

## Minimal use

```cpp
#include <phyz/libphyz.hpp>

phyz::engine::Simulation simulation(phyz::engine::UnitSystem::astronomical());
simulation.set_integrator<phyz::engine::Yoshida4Fixed>();
simulation.forces().add<phyz::engine::PairwiseGravity>(
    simulation.units().gravitationalConstant);
simulation.add_body({"Sun", phyz::engine::BodyKind::Star, 1.0, 1.0});
simulation.advance_to(1.0);
```

All public values use the `UnitSystem` attached to the simulation. The astronomical preset is AU, solar mass and Julian year. The library deliberately contains no Windows, OpenGL, Godot, UI or mission-rule dependencies.
