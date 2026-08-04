# Changelog

## [Unreleased] — 2026-08-04

### Added
- Added `libphyz` 0.1, a frontend-independent C++20 astrodynamics library with stable body IDs and explicit unit systems
- Added composable Newtonian, Plummer and Paczynski-Wiita gravity plus constant-acceleration force models
- Added Leapfrog-2 and Yoshida-4 integrators, invariant diagnostics, osculating elements and physical event detectors
- Added deterministic versioned snapshots and immutable branch trajectory prediction with time-tagged impulses
- Added a Godot 4.6 GDExtension frontend with five physics-derived missions, maneuver nodes, trajectory preview, scoring and save/restore
- Added CMake package export, CTest coverage, reproducible Godot bootstrap/build/export scripts and a validated Windows release package
- Added the three-chapter Chinese campaign **Ember Route / 余烬航线**, with story briefs, tutorials, debriefs, progression and best-score persistence
- Added a redesigned 1440×900 cockpit, orbit camera, focus controls, role labels, target guidance, live objective metrics and tested navigation suggestions
- Added campaign-solution regression tests, a full first-mission UI smoke test and a tracked gameplay screenshot

### Changed
- Replaced the scripted 220-world explorer with a 32-body self-consistent planetary N-body system
- Removed analytical position rewrites, vacuum damping, speed caps, visual-surface bounce, and position snapping
- Modeled spacecraft braking as a finite retrograde burn
- Separated visual radius from physical collision/orbital calculations in the generated system
- Added integrator, active-body count, timestep, and linear-momentum residual to the HUD

### Fixed
- Made Paczynski-Wiita black-hole pair forces equal and opposite to preserve linear momentum
- Made reported potential energy consistent with the active strong-field force approximation
- Corrected automatic physical-radius scaling for sub-Jovian planets and minor bodies
- Extended headless tests to verify live accelerations, conservative energy drift, and momentum residual
- Corrected the physics-XY to Godot-XZ world mapping so orbital systems are presented from a playable camera angle
- Rebalanced the gravity-assist objective from an unreachable speed increase to a verified finite-budget exit-speed target
- Replaced the narrow debug-style mission panel with a scrollable, readable game interface and explicit player/target identification

## [1.0.0] — 2026-06-27

### Added
- Initial open-source release
- N-body gravitational simulation with 4th-order Yoshida symplectic integrator
- 7 celestial body types: star, planet, black_hole, neutron_star, white_dwarf, minor_body, spacecraft
- 6 built-in scenes: chaotic three-body, figure-eight, Lagrangian triangle, hierarchical triple, gravity assist, procedural universe
- Collision merging with momentum conservation
- Black hole physics: event horizon, ISCO, accretion disk, Paczynski-Wiita pseudo-Newtonian potential
- Tidal disruption with Roche limit estimation
- Chaos shadow system (1e-6 AU perturbation with parallel integration)
- Interactive edit mode (drag bodies with mouse)
- 3D OpenGL rendering with procedural starfield, trajectory trails, gravitational field arrows
- INI-based custom initial conditions system
- 6 preset scenario files
- Self-test system for conservation law validation
- Headless mode for physics validation (`--self-test`)

### Past development history

See `DEVELOPMENT_LOG.md` for detailed development journey and technical notes.
