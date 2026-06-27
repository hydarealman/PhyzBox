# Changelog

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
