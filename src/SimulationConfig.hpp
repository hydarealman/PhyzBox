#pragma once

#include "Math3D.hpp"

#include <optional>
#include <string>
#include <vector>

namespace phyz {

struct BodyInitialConfig {
    std::optional<std::string> name;
    std::optional<double> mass;
    std::optional<double> radius;
    std::optional<double> physicalRadius;
    std::optional<Color> color;
    std::optional<Vec3> position;
    std::optional<Vec3> velocity;
};

struct InitialConditionConfig {
    bool enabled = false;
    int bodyCount = 3;
    std::optional<double> physicsDt;
    std::optional<double> cameraDistance;
    std::optional<double> softening;
    std::string sourcePath;
    std::vector<BodyInitialConfig> bodies;
};

InitialConditionConfig loadInitialConditionConfig(const std::string& path);
InitialConditionConfig loadInitialConditionConfigFromDefaultLocations();

} // namespace phyz
