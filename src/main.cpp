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
        double closestPair = std::numeric_limits<double>::infinity();
        for (int i = 0; i < 20000; ++i) {
            system.step(dt);
            maxAbsDrift = std::max(maxAbsDrift, std::abs(system.energyDrift()));
            maxAbsAngularDrift = std::max(maxAbsAngularDrift, std::abs(system.angularMomentumDrift()));
            closestPair = std::min(closestPair, system.minSeparation());
        }

        std::cout << system.scenarioName()
                  << " | simulated time " << system.time()
                  << " yr | closest " << closestPair
                  << " AU | max energy drift " << (maxAbsDrift * 100.0)
                  << "% | max angular drift " << (maxAbsAngularDrift * 100.0) << "%\n";

        if (!std::isfinite(maxAbsDrift) || maxAbsDrift > 5.0e-4 ||
            !std::isfinite(maxAbsAngularDrift) || maxAbsAngularDrift > 1.0e-8) {
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

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return runSelfTest();
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
