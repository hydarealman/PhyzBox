#pragma once

#include "phyz/simulation/simulation.hpp"

#include <vector>

namespace phyz::engine {

struct ImpulseManeuver {
    double time = 0.0;
    BodyId body{};
    Vec3d deltaVelocity{};
};

struct TrajectoryPoint {
    double time = 0.0;
    Vec3d position{};
    Vec3d velocity{};
};

struct BodyTrajectory {
    BodyId body{};
    std::vector<TrajectoryPoint> points;
};

struct TrajectoryPrediction {
    StepStatus status = StepStatus::Success;
    double startTime = 0.0;
    double endTime = 0.0;
    std::vector<BodyTrajectory> trajectories;
    std::vector<Event> events;
};

[[nodiscard]] TrajectoryPrediction predict_trajectory(
    const Simulation& source,
    double endTime,
    double samplePeriod,
    std::vector<ImpulseManeuver> maneuvers = {});

} // namespace phyz::engine
