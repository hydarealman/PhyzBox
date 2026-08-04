#pragma once

#include "phyz/core/body.hpp"
#include "phyz/dynamics/force_model.hpp"

#include <optional>
#include <span>

namespace phyz::engine {

struct InvariantReport {
    double kineticEnergy = 0.0;
    std::optional<double> potentialEnergy;
    std::optional<double> mechanicalEnergy;
    Vec3d linearMomentum{};
    Vec3d angularMomentum{};
    Vec3d centerOfMass{};
    Vec3d centerOfMassVelocity{};
    double totalInertialMass = 0.0;
    double momentumResidual = 0.0;
};

[[nodiscard]] InvariantReport calculate_invariants(
    std::span<const BodyState> bodies,
    const ForcePipeline& forces,
    double time);

enum class ConicType {
    Circular,
    Elliptic,
    Parabolic,
    Hyperbolic,
    Degenerate,
};

struct OrbitalElements {
    ConicType conic = ConicType::Degenerate;
    double semiMajorAxis = 0.0;
    double eccentricity = 0.0;
    double inclination = 0.0;
    double longitudeOfAscendingNode = 0.0;
    double argumentOfPeriapsis = 0.0;
    double trueAnomaly = 0.0;
    double periapsisDistance = 0.0;
    double apoapsisDistance = 0.0;
    double specificOrbitalEnergy = 0.0;
    Vec3d specificAngularMomentum{};
    Vec3d eccentricityVector{};
};

[[nodiscard]] std::optional<OrbitalElements> cartesian_to_orbital_elements(
    Vec3d relativePosition,
    Vec3d relativeVelocity,
    double standardGravitationalParameter);

} // namespace phyz::engine
