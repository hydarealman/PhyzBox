#include "Application.hpp"
#include "NBodySystem.hpp"
#include "SimulationConfig.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

int runSelfTest() {
    using phyz::NBodySystem;
    using phyz::Scenario;

    const Scenario scenarios[] = {
        Scenario::TrisolarisChaos,
        Scenario::FigureEight,
        Scenario::InclinedDance,
        Scenario::HierarchicalTriple,
        Scenario::GravityAssist,
    };

    bool ok = true;
    for (Scenario scenario : scenarios) {
        NBodySystem system;
        system.reset(scenario);

        const double dt = system.recommendedTimeStep();
        double maxAbsDrift = 0.0;
        double maxAbsAngularDrift = 0.0;
        double maxLinearMomentumError = 0.0;
        double closestPair = std::numeric_limits<double>::infinity();
        for (int i = 0; i < 20000; ++i) {
            system.step(dt);
            maxAbsDrift = std::max(maxAbsDrift, std::abs(system.energyDrift()));
            maxAbsAngularDrift = std::max(maxAbsAngularDrift, std::abs(system.angularMomentumDrift()));
            maxLinearMomentumError = std::max(maxLinearMomentumError, system.linearMomentumError());
            closestPair = std::min(closestPair, system.minSeparation());
        }

        std::cout << system.scenarioName()
                  << " | simulated time " << system.time()
                  << " yr | closest " << closestPair
                  << " AU | max energy drift " << (maxAbsDrift * 100.0)
                  << "% | max angular drift " << (maxAbsAngularDrift * 100.0)
                  << "% | momentum residual " << maxLinearMomentumError << "\n";

        if (!std::isfinite(maxAbsDrift) || maxAbsDrift > 5.0e-4 ||
            !std::isfinite(maxAbsAngularDrift) || maxAbsAngularDrift > 1.0e-8 ||
            !std::isfinite(maxLinearMomentumError) || maxLinearMomentumError > 1.0e-10) {
            ok = false;
        }
    }

    std::cout << (ok ? "Self-test passed\n" : "Self-test failed\n");
    return ok ? 0 : 2;
}

int runConfigSelfTest(const std::string& path) {
    phyz::InitialConditionConfig config = phyz::loadInitialConditionConfig(path);
    if (!config.enabled) {
        std::cerr << "No usable config found at " << path << "\n";
        return 2;
    }

    phyz::NBodySystem system;
    system.reset(config);

    const std::size_t initialBodyCount = system.bodies().size();
    const double dt = system.recommendedTimeStep();
    double maxAbsDrift = 0.0;
    double maxAbsAngularDrift = 0.0;
    double closestPair = std::numeric_limits<double>::infinity();
    for (int i = 0; i < 5000; ++i) {
        system.step(dt);
        maxAbsDrift = std::max(maxAbsDrift, std::abs(system.energyDrift()));
        maxAbsAngularDrift = std::max(maxAbsAngularDrift, std::abs(system.angularMomentumDrift()));
        closestPair = std::min(closestPair, system.minSeparation());
    }

    std::cout << "Config " << path
              << " | bodies " << system.bodies().size()
              << " | events " << system.events().size()
              << " | simulated time " << system.time()
              << " yr | closest " << closestPair
              << " AU | max energy drift " << (maxAbsDrift * 100.0)
              << "% | max angular drift " << (maxAbsAngularDrift * 100.0) << "%\n";

    const bool eventful = system.bodies().size() != initialBodyCount || !system.events().empty();
    const bool ok = std::isfinite(maxAbsDrift) && std::isfinite(maxAbsAngularDrift) &&
        (eventful || maxAbsDrift < 2.0e-3) &&
        (eventful || maxAbsAngularDrift < 1.0e-8);
    std::cout << (ok ? "Config self-test passed\n" : "Config self-test failed\n");
    return ok ? 0 : 2;
}

int runExplorerSelfTest() {
    phyz::NBodySystem system;
    system.reset(phyz::Scenario::ProceduralUniverse);

    std::size_t dynamicallyAcceleratedBodies = 0;
    for (const phyz::Body& body : system.bodies()) {
        if (body.type != phyz::BodyType::Spacecraft && phyz::length(body.acceleration) > 1.0e-10) {
            ++dynamicallyAcceleratedBodies;
        }
    }

    double maxConservativeEnergyDrift = 0.0;
    double maxMomentumResidual = 0.0;
    const double dt = system.recommendedTimeStep();
    for (int i = 0; i < 2500; ++i) {
        system.step(dt);
        maxConservativeEnergyDrift = std::max(maxConservativeEnergyDrift, std::abs(system.energyDrift()));
        maxMomentumResidual = std::max(maxMomentumResidual, system.linearMomentumError());
    }

    phyz::ExplorerControlState control;
    control.thrustDirection = phyz::normalized(phyz::Vec3{1.0, 0.25, 0.05});
    control.boost = true;
    system.setExplorerControlState(control);

    for (int i = 0; i < 2500; ++i) {
        system.step(dt);
    }

    const bool ok = system.bodies().size() >= 24 &&
        dynamicallyAcceleratedBodies + 1 >= system.bodies().size() &&
        system.spacecraftIndex() >= 0 &&
        system.explorerNearestPlanetIndex() >= 0 &&
        std::isfinite(system.spacecraftSpeed()) &&
        std::isfinite(system.explorerLocalGravity()) &&
        system.explorerNearestPlanetDistance() > 0.0 &&
        maxConservativeEnergyDrift < 1.0e-5 &&
        maxMomentumResidual < 1.0e-10;

    std::cout << "Explorer mode | bodies " << system.bodies().size()
              << " | live gravity bodies " << dynamicallyAcceleratedBodies
              << " | max conservative energy drift " << (maxConservativeEnergyDrift * 100.0) << "%"
              << " | momentum residual " << maxMomentumResidual
              << " | ship speed " << system.spacecraftSpeed()
              << " AU/yr | nearest planet distance "
              << system.explorerNearestPlanetDistance()
              << " AU | net gravity " << system.explorerLocalGravity()
              << " AU/yr^2\n";
    std::cout << (ok ? "Explorer self-test passed\n" : "Explorer self-test failed\n");
    return ok ? 0 : 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return runSelfTest();
    }
    if (argc > 1 && std::string(argv[1]) == "--self-test-explorer") {
        return runExplorerSelfTest();
    }
    if (argc > 2 && std::string(argv[1]) == "--self-test-config") {
        return runConfigSelfTest(argv[2]);
    }

    phyz::Application app;
    if (!app.initialize()) {
        std::cerr << "Failed to initialize PhyzBox.\n";
        return 1;
    }

    app.run();
    return 0;
}
