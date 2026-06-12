#pragma once

#include "Math3D.hpp"

#include <optional>
#include <string>
#include <vector>

namespace phyz {

enum class BodyType {
    Star,
    Planet,
    BlackHole,
    NeutronStar,
    WhiteDwarf,
    MinorBody,
};

const char* bodyTypeName(BodyType type);

struct BodyInitialConfig {
    std::optional<std::string> name;
    std::optional<BodyType> type;
    std::optional<double> mass;
    std::optional<double> radius;
    std::optional<double> physicalRadius;
    std::optional<double> density;
    std::optional<double> temperature;
    std::optional<double> luminosity;
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
