# Changelog

## [Unreleased] — 2026-08-04

### Time-control usability — 2026-08-12
- Added a compact bottom time-rate slider with nine labeled speeds and synchronized keyboard/play controls
- Displayed the effective automatic-slowdown rate beside the selected rate during close encounters
- Fixed manual slider changes being silently clamped to one minute per second by launch-event auto slowdown

### Voyager historical solar system — 2026-08-11
- Replaced the default campaign cockpit with a HUD-free Voyager 1 historical-playback scene
- Imported JPL DE440s and official Voyager 1/2 SPK states for 1977–2030 with double-precision Hermite interpolation
- Added time-rate controls, automatic encounter slowdown, five history jumps, free date stepping and follow/system cameras
- Added a code-built Voyager spacecraft and dual-scale rendering that preserves physical angular diameter near planets
- Removed every CanvasLayer, button, panel, picker and timeline from the historical runtime view
- Imported 83,337 Hipparcos stars with ICRS directions, V magnitudes and B−V-derived colors
- Added a J2000-oriented procedural Milky Way, planet surfaces, clouds and atmosphere shaders without generated textures
- Added an audience-facing aerospace color grade with a one-key physical-exposure comparison mode
- Gave Mars, Saturn and Neptune distinct enhanced-natural-color palettes and replaced pure black space with a restrained deep-navy field
- Rebuilt Saturn's rings as a thin procedural annulus with band structure and the Cassini Division
- Added auditable source/hash metadata, deterministic import tools, historical-playback tests and new visual captures

### Visual overhaul — 2026-08-05
- Rebuilt the cockpit around a cinematic top route bar, compact objective card and bottom maneuver dock
- Moved integrator, orbital elements and persistence controls into an optional professional telemetry panel
- Replaced the player sphere with a velocity-aligned component spacecraft featuring solar arrays, radiators, antenna, RCS pods and four electric thrusters
- Added a distinct orbital-station visual for rendezvous targets and reduced oversized world labels
- Added a cinematic mission briefing with a fully procedural Godot shader backdrop; no generated chapter textures
- Added filmic lighting, restrained navigation grid styling, stronger color semantics and visual-regression screenshots
- Extended UI smoke tests to verify compact layout, collapsed telemetry, chapter art and component-built spacecraft

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
