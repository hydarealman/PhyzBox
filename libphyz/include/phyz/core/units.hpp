#pragma once

#include <numbers>

namespace phyz::engine {

struct UnitSystem {
    double lengthInMeters = 1.0;
    double massInKilograms = 1.0;
    double timeInSeconds = 1.0;
    double gravitationalConstant = 6.67430e-11;

    static constexpr UnitSystem si() { return {}; }

    static constexpr UnitSystem astronomical() {
        return {
            149597870700.0,
            1.98847e30,
            31557600.0,
            4.0 * std::numbers::pi * std::numbers::pi,
        };
    }
};

} // namespace phyz::engine
