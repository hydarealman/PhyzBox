#include "phyz/libphyz.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <string>

using namespace phyz::engine;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(double actual, double expected, double tolerance) {
    return std::abs(actual - expected) <= tolerance;
}

Simulation circular_two_body(double step, bool yoshida) {
    const UnitSystem units = UnitSystem::astronomical();
    Simulation simulation(units);
    simulation.set_fixed_step(step);
    if (yoshida) {
        simulation.set_integrator<Yoshida4Fixed>();
    } else {
        simulation.set_integrator<Leapfrog2Fixed>();
    }
    simulation.forces().add<PairwiseGravity>(units.gravitationalConstant);

    constexpr double planetMass = 3.003e-6;
    constexpr double separation = 1.0;
    const double totalMass = 1.0 + planetMass;
    const double angularSpeed = std::sqrt(units.gravitationalConstant * totalMass /
        (separation * separation * separation));
    const double starRadius = separation * planetMass / totalMass;
    const double planetRadius = separation / totalMass;

    simulation.add_body({
        "Primary", BodyKind::Star, 1.0, 1.0, 0.00465047, 0.0, 0.03,
        {-starRadius, 0.0, 0.0}, {0.0, -angularSpeed * starRadius, 0.0},
    });
    simulation.add_body({
        "Planet", BodyKind::Planet, planetMass, planetMass, 4.2635e-5, 0.0, 0.01,
        {planetRadius, 0.0, 0.0}, {0.0, angularSpeed * planetRadius, 0.0},
    });
    return simulation;
}

double one_orbit_position_error(double step, bool yoshida) {
    Simulation simulation = circular_two_body(step, yoshida);
    const Vec3d initial = simulation.bodies()[1].position;
    const double period = 1.0 / std::sqrt(1.0 + 3.003e-6);
    const AdvanceReport report = simulation.advance_to(period);
    check(report.status == StepStatus::Success, "circular orbit integration succeeds");
    return length(simulation.bodies()[1].position - initial);
}

void test_free_particle() {
    Simulation simulation;
    simulation.set_fixed_step(0.01);
    simulation.set_integrator<Leapfrog2Fixed>();
    simulation.add_body({"Free", BodyKind::Spacecraft, 0.0, 0.0, 0.0, 0.0, 0.01, {1.0, 2.0, 3.0}, {4.0, -2.0, 0.5}});
    const AdvanceReport report = simulation.advance_to(2.0);
    check(report.status == StepStatus::Success, "free particle advances");
    const Vec3d position = simulation.bodies()[0].position;
    check(length(position - Vec3d{9.0, -2.0, 4.0}) < 1.0e-11, "free particle follows exact linear motion");
}

void test_integrator_convergence() {
    const double leapfrogCoarse = one_orbit_position_error(0.01, false);
    const double leapfrogFine = one_orbit_position_error(0.005, false);
    const double yoshidaCoarse = one_orbit_position_error(0.01, true);
    const double yoshidaFine = one_orbit_position_error(0.005, true);

    check(leapfrogCoarse / leapfrogFine > 3.5, "Leapfrog exhibits second-order convergence");
    check(yoshidaCoarse / yoshidaFine > 12.0, "Yoshida exhibits fourth-order convergence");
    check(yoshidaFine < leapfrogFine * 0.01, "Yoshida accuracy exceeds Leapfrog at equal step");
}

void test_invariants() {
    Simulation simulation = circular_two_body(0.001, true);
    const InvariantReport initial = simulation.invariants();
    simulation.advance_to(10.0);
    const InvariantReport final = simulation.invariants();
    check(initial.mechanicalEnergy.has_value() && final.mechanicalEnergy.has_value(), "conservative energy is available");
    const double energyDrift = std::abs((*final.mechanicalEnergy - *initial.mechanicalEnergy) / *initial.mechanicalEnergy);
    check(energyDrift < 1.0e-10, "ten-orbit Yoshida energy drift is bounded");
    check(final.momentumResidual < 1.0e-12, "pairwise gravity preserves linear momentum");
    check(length(final.centerOfMass) < 1.0e-12, "barycentric frame remains centered");
}

void test_orbital_elements() {
    const double mu = 4.0 * std::numbers::pi * std::numbers::pi;
    const auto circular = cartesian_to_orbital_elements({1.0, 0.0, 0.0}, {0.0, 2.0 * std::numbers::pi, 0.0}, mu);
    check(circular.has_value(), "circular orbital elements are computed");
    check(circular && near(circular->semiMajorAxis, 1.0, 1.0e-12), "circular semi-major axis is correct");
    check(circular && circular->eccentricity < 1.0e-12, "circular eccentricity is zero");
    check(circular && circular->conic == ConicType::Circular, "circular conic classification is correct");

    const auto hyperbolic = cartesian_to_orbital_elements({1.0, 0.0, 0.0}, {0.0, 10.0, 0.0}, mu);
    check(hyperbolic && hyperbolic->conic == ConicType::Hyperbolic, "hyperbolic state is classified correctly");
    check(hyperbolic && hyperbolic->semiMajorAxis < 0.0, "hyperbolic semi-major axis is negative");
}

void test_event_localization() {
    Simulation simulation(UnitSystem::si());
    simulation.set_integrator<Leapfrog2Fixed>();
    simulation.set_fixed_step(1.0);
    simulation.add_detector(std::make_unique<CollisionDetector>());
    simulation.add_body({"A", BodyKind::MinorBody, 0.0, 0.0, 0.1, 0.0, 0.1, {-1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}});
    simulation.add_body({"B", BodyKind::MinorBody, 0.0, 0.0, 0.1, 0.0, 0.1, {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}});
    const AdvanceReport report = simulation.step(1.0);
    check(report.events.size() == 1, "collision is detected inside a timestep");
    check(!report.events.empty() && near(report.events[0].time, 0.9, 1.0e-12), "first physical contact time is localized inside the step");
}

void test_strong_field_events() {
    Simulation simulation(UnitSystem::si());
    simulation.set_integrator<Leapfrog2Fixed>();
    simulation.add_detector(std::make_unique<EventHorizonDetector>());
    simulation.add_body({"Black hole", BodyKind::BlackHole, 0.0, 0.0, 0.01, 0.2, 0.1, {}, {}});
    simulation.add_body({"Probe", BodyKind::Spacecraft, 0.0, 0.0, 0.0, 0.0, 0.01, {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}});
    const AdvanceReport report = simulation.step(1.0);
    check(report.events.size() == 1 && report.events[0].type == EventType::EventHorizonCrossing,
          "event-horizon crossing is detected");
    check(!report.events.empty() && near(report.events[0].time, 0.8, 1.0e-12),
          "event-horizon entry time is localized");
}

void test_paczynski_wiita_symmetry() {
    ForcePipeline forces;
    forces.add<PairwiseGravity>(1.0, GravityLaw::PaczynskiWiita);
    std::vector<BodyState> bodies{
        {{1}, 10.0, 10.0, 0.1, 0.01, {-1.0, 0.0, 0.0}, {}, {}},
        {{2}, 2.0, 2.0, 0.1, 0.0, {2.0, 0.0, 0.0}, {}, {}},
    };
    std::vector<Vec3d> accelerations(2);
    forces.calculate(bodies, 0.0, accelerations);
    const Vec3d netForce = accelerations[0] * bodies[0].inertialMass + accelerations[1] * bodies[1].inertialMass;
    check(length(netForce) < 1.0e-13, "Paczynski-Wiita pair force is equal and opposite");
}

void test_snapshot_roundtrip() {
    Simulation original = circular_two_body(0.001, true);
    original.advance_to(0.25);
    const std::string serialized = original.serialize_snapshot();
    std::string error;
    auto restored = Simulation::deserialize_snapshot(serialized, &error);
    check(restored.has_value(), "snapshot restores: " + error);
    if (!restored) {
        return;
    }
    check(restored->bodies().size() == original.bodies().size(), "snapshot preserves body count");
    check(length(restored->bodies()[1].position - original.bodies()[1].position) < 1.0e-15, "snapshot preserves double-precision position");
    original.advance_to(0.5);
    restored->advance_to(0.5);
    check(length(restored->bodies()[1].position - original.bodies()[1].position) < 1.0e-13, "restored simulation continues deterministically");
}

void test_nonconservative_diagnostics() {
    Simulation simulation;
    const BodyId ship = simulation.add_body({"Ship", BodyKind::Spacecraft, 0.0, 0.0, 0.0, 0.0, 0.01, {}, {}});
    simulation.forces().add<ConstantAcceleration>(ship, Vec3d{1.0, 0.0, 0.0});
    check(!simulation.invariants().mechanicalEnergy.has_value(), "non-conservative force disables mechanical-energy error claim");
    simulation.step(1.0);
    check(near(simulation.bodies()[0].velocity.x, 1.0, 1.0e-12), "constant acceleration is integrated");
}

void test_trajectory_prediction() {
    Simulation simulation = circular_two_body(0.001, true);
    const BodyId planet = simulation.bodies()[1].id;
    const Vec3d originalPosition = simulation.bodies()[1].position;
    const Vec3d originalVelocity = simulation.bodies()[1].velocity;
    const TrajectoryPrediction prediction = predict_trajectory(
        simulation,
        0.1,
        0.01,
        {{0.05, planet, {0.0, 0.1, 0.0}}});
    check(prediction.status == StepStatus::Success, "trajectory prediction succeeds");
    check(prediction.trajectories.size() == 2 && prediction.trajectories[1].points.size() == 11,
          "trajectory prediction returns deterministic samples");
    check(length(simulation.bodies()[1].position - originalPosition) < 1.0e-15 &&
          length(simulation.bodies()[1].velocity - originalVelocity) < 1.0e-15,
          "trajectory prediction does not mutate the source simulation");
}

} // namespace

int main() {
    test_free_particle();
    test_integrator_convergence();
    test_invariants();
    test_orbital_elements();
    test_event_localization();
    test_strong_field_events();
    test_paczynski_wiita_symmetry();
    test_snapshot_roundtrip();
    test_nonconservative_diagnostics();
    test_trajectory_prediction();

    if (failures == 0) {
        std::cout << "libphyz validation passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " libphyz validation test(s) failed\n";
    return EXIT_FAILURE;
}
