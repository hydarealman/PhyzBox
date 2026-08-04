#include "phyz/libphyz.hpp"

#include <iostream>
#include <numbers>

int main() {
    using namespace phyz::engine;
    Simulation simulation(UnitSystem::astronomical());
    simulation.forces().add<PairwiseGravity>(simulation.units().gravitationalConstant);
    simulation.set_integrator<Yoshida4Fixed>();
    simulation.set_fixed_step(1.0e-4);

    simulation.add_body({"Sun", BodyKind::Star, 1.0, 1.0, 0.00465047, 0.0, 0.03, {}, {}});
    simulation.add_body({"Earth", BodyKind::Planet, 3.003e-6, 3.003e-6, 4.2635e-5, 0.0, 0.01,
                         {1.0, 0.0, 0.0}, {0.0, 2.0 * std::numbers::pi, 0.0}});

    simulation.advance_to(1.0);
    const InvariantReport report = simulation.invariants();
    std::cout << "time=" << simulation.time()
              << " position=" << simulation.bodies()[1].position.x << ',' << simulation.bodies()[1].position.y
              << " momentum_residual=" << report.momentumResidual << '\n';
}
