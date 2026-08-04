#pragma once

#include "phyz/math/vec3.hpp"

#include <cstdint>
#include <string>

namespace phyz::engine {

struct BodyId {
    std::uint64_t value = 0;
    constexpr auto operator<=>(const BodyId&) const = default;
};

enum class BodyKind {
    Star,
    Planet,
    BlackHole,
    NeutronStar,
    WhiteDwarf,
    MinorBody,
    Spacecraft,
};

struct BodyState {
    BodyId id{};
    double gravitationalMass = 1.0;
    double inertialMass = 1.0;
    double physicalRadius = 0.0;
    double schwarzschildRadius = 0.0;
    Vec3d position{};
    Vec3d velocity{};
    Vec3d acceleration{};
};

struct BodyMetadata {
    BodyId id{};
    std::string name;
    BodyKind kind = BodyKind::Star;
    double displayRadius = 0.01;
};

struct BodyDefinition {
    std::string name;
    BodyKind kind = BodyKind::Star;
    double gravitationalMass = 1.0;
    double inertialMass = 1.0;
    double physicalRadius = 0.0;
    double schwarzschildRadius = 0.0;
    double displayRadius = 0.01;
    Vec3d position{};
    Vec3d velocity{};
};

} // namespace phyz::engine
