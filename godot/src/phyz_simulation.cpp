#include "phyz_simulation.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>

namespace godot {
namespace {

using phyz::engine::BodyDefinition;
using phyz::engine::BodyId;
using phyz::engine::BodyKind;
using phyz::engine::ConstantAcceleration;
using phyz::engine::PairwiseGravity;
using phyz::engine::GravityLaw;
using phyz::engine::UnitSystem;
using phyz::engine::Vec3d;

PackedFloat64Array packed(Vec3d value) {
    PackedFloat64Array result;
    result.resize(3);
    result[0] = value.x;
    result[1] = value.y;
    result[2] = value.z;
    return result;
}

Dictionary event_dictionary(const phyz::engine::Event& event) {
    Dictionary result;
    result["type"] = static_cast<int>(event.type);
    result["time"] = event.time;
    result["primary_id"] = static_cast<std::int64_t>(event.primary.value);
    result["secondary_id"] = static_cast<std::int64_t>(event.secondary.value);
    result["value"] = event.value;
    result["threshold"] = event.threshold;
    return result;
}

} // namespace

PhyzSimulation::PhyzSimulation() {
    load_mission(0);
}

void PhyzSimulation::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_mission", "mission_index"), &PhyzSimulation::load_mission);
    ClassDB::bind_method(D_METHOD("advance", "years"), &PhyzSimulation::advance);
    ClassDB::bind_method(D_METHOD("advance_controlled", "years", "x", "y", "z", "throttle"), &PhyzSimulation::advance_controlled);
    ClassDB::bind_method(D_METHOD("apply_impulse", "body_id", "x", "y", "z"), &PhyzSimulation::apply_impulse);
    ClassDB::bind_method(D_METHOD("schedule_impulse", "body_id", "time", "x", "y", "z"), &PhyzSimulation::schedule_impulse);
    ClassDB::bind_method(D_METHOD("cancel_scheduled_impulse"), &PhyzSimulation::cancel_scheduled_impulse);
    ClassDB::bind_method(D_METHOD("get_scheduled_impulse"), &PhyzSimulation::get_scheduled_impulse);
    ClassDB::bind_method(D_METHOD("predict", "body_id", "x", "y", "z", "burn_time", "duration", "sample_period"), &PhyzSimulation::predict);
    ClassDB::bind_method(D_METHOD("get_bodies"), &PhyzSimulation::get_bodies);
    ClassDB::bind_method(D_METHOD("get_invariants"), &PhyzSimulation::get_invariants);
    ClassDB::bind_method(D_METHOD("get_orbital_elements", "body_id", "primary_id"), &PhyzSimulation::get_orbital_elements);
    ClassDB::bind_method(D_METHOD("get_mission_definition"), &PhyzSimulation::get_mission_definition);
    ClassDB::bind_method(D_METHOD("evaluate_mission"), &PhyzSimulation::evaluate_mission);
    ClassDB::bind_method(D_METHOD("save_snapshot"), &PhyzSimulation::save_snapshot);
    ClassDB::bind_method(D_METHOD("load_snapshot", "text"), &PhyzSimulation::load_snapshot);
    ClassDB::bind_method(D_METHOD("get_time"), &PhyzSimulation::get_time);
    ClassDB::bind_method(D_METHOD("get_fixed_step"), &PhyzSimulation::get_fixed_step);
    ClassDB::bind_method(D_METHOD("get_integrator_name"), &PhyzSimulation::get_integrator_name);
}

void PhyzSimulation::add_gravity(double softening) {
    simulation_.forces().add<PairwiseGravity>(
        simulation_.units().gravitationalConstant,
        softening > 0.0 ? GravityLaw::PlummerSoftened : GravityLaw::Newtonian,
        softening);
    simulation_.add_detector(std::make_unique<phyz::engine::CollisionDetector>());
    simulation_.add_detector(std::make_unique<phyz::engine::CloseApproachDetector>(0.03));
    simulation_.add_detector(std::make_unique<phyz::engine::EventHorizonDetector>());
}

void PhyzSimulation::load_mission(int missionIndex) {
    missionIndex_ = std::clamp(missionIndex, 0, 4);
    simulation_ = phyz::engine::Simulation(UnitSystem::astronomical());
    thrustForce_ = nullptr;
    simulation_.set_integrator<phyz::engine::Yoshida4Fixed>();
    deltaVSpent_ = 0.0;
    scheduledImpulse_.reset();
    playerBodyId_ = 0;
    targetBodyId_ = 0;
    primaryBodyId_ = 0;
    switch (missionIndex_) {
    case 0: build_two_body_transfer(); break;
    case 1: build_rendezvous(); break;
    case 2: build_gravity_assist(); break;
    case 3: build_asteroid_deflection(); break;
    case 4: build_chaos_survival(); break;
    }
    if (const auto* player = simulation_.find_body(BodyId{static_cast<std::uint64_t>(playerBodyId_)})) {
        initialPlayerSpeed_ = phyz::engine::length(player->velocity);
    }
    attach_thrust_force();
}

void PhyzSimulation::attach_thrust_force() {
    thrustForce_ = nullptr;
    for (const auto& model : simulation_.forces().models()) {
        if (auto* acceleration = dynamic_cast<ConstantAcceleration*>(model.get());
            acceleration && acceleration->target().value == static_cast<std::uint64_t>(playerBodyId_)) {
            thrustForce_ = acceleration;
            thrustForce_->set_acceleration({});
            return;
        }
    }
    thrustForce_ = &simulation_.forces().add<ConstantAcceleration>(
        BodyId{static_cast<std::uint64_t>(playerBodyId_)}, Vec3d{});
}

void PhyzSimulation::build_two_body_transfer() {
    simulation_.set_fixed_step(0.0002);
    deltaVBudget_ = 3.5;
    missionDeadline_ = 2.2;
    add_gravity();
    primaryBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Helios", BodyKind::Star, 1.0, 1.0, 0.00465, 0.0, 0.08, {}, {},
    }).value);
    const double planetRadius = 0.72;
    const double planetPhase = -0.82;
    simulation_.add_body({
        "Cinder", BodyKind::Planet, 3.0e-6, 3.0e-6, 4.3e-5, 0.0, 0.052,
        {planetRadius * std::cos(planetPhase), planetRadius * std::sin(planetPhase), 0.0},
        {-std::sqrt(simulation_.units().gravitationalConstant / planetRadius) * std::sin(planetPhase),
          std::sqrt(simulation_.units().gravitationalConstant / planetRadius) * std::cos(planetPhase), 0.0},
    });
    playerBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Courier", BodyKind::Spacecraft, 0.0, 0.0, 8.0e-8, 0.0, 0.025,
        {1.0, 0.0, 0.0}, {0.0, 2.0 * std::numbers::pi, 0.0},
    }).value);
    const double radius = 1.5;
    const double phase = 0.58;
    targetBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Aster Station", BodyKind::Planet, 1.0e-6, 1.0e-6, 5.0e-5, 0.0, 0.035,
        {radius * std::cos(phase), radius * std::sin(phase), 0.0},
        {-std::sqrt(simulation_.units().gravitationalConstant / radius) * std::sin(phase),
          std::sqrt(simulation_.units().gravitationalConstant / radius) * std::cos(phase), 0.0},
    }).value);
}

void PhyzSimulation::build_rendezvous() {
    simulation_.set_fixed_step(0.00015);
    deltaVBudget_ = 1.4;
    missionDeadline_ = 0.9;
    add_gravity();
    primaryBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Primary", BodyKind::Star, 1.0, 1.0, 0.00465, 0.0, 0.08, {}, {},
    }).value);
    playerBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Rescue", BodyKind::Spacecraft, 0.0, 0.0, 8.0e-8, 0.0, 0.024,
        {1.0, 0.0, 0.0}, {0.0, 6.18, 0.0},
    }).value);
    const double phase = 0.35;
    targetBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Drifting Probe", BodyKind::Spacecraft, 0.0, 0.0, 8.0e-8, 0.0, 0.022,
        {std::cos(phase), std::sin(phase), 0.0}, {-2.0 * std::numbers::pi * std::sin(phase), 2.0 * std::numbers::pi * std::cos(phase), 0.0},
    }).value);
}

void PhyzSimulation::build_gravity_assist() {
    simulation_.set_fixed_step(0.00005);
    deltaVBudget_ = 1.0;
    missionDeadline_ = 0.55;
    add_gravity(5.0e-6);
    primaryBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Sun", BodyKind::Star, 1.0, 1.0, 0.00465, 0.0, 0.08, {}, {},
    }).value);
    targetBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Giant", BodyKind::Planet, 0.0015, 0.0015, 0.0005, 0.0, 0.055,
        {1.6, 0.0, 0.0}, {0.0, 4.98, 0.0},
    }).value);
    playerBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Daedalus", BodyKind::Spacecraft, 0.0, 0.0, 8.0e-8, 0.0, 0.023,
        {1.05, -0.65, 0.05}, {3.65, 9.75, -0.2},
    }).value);
}

void PhyzSimulation::build_asteroid_deflection() {
    simulation_.set_fixed_step(0.0001);
    deltaVBudget_ = 0.35;
    missionDeadline_ = 0.7;
    add_gravity(2.0e-6);
    primaryBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Star", BodyKind::Star, 1.0, 1.0, 0.00465, 0.0, 0.08, {}, {},
    }).value);
    targetBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Haven", BodyKind::Planet, 3.0e-6, 3.0e-6, 4.3e-5, 0.0, 0.04,
        {1.0, 0.0, 0.0}, {0.0, 2.0 * std::numbers::pi, 0.0},
    }).value);
    playerBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Threat 2049", BodyKind::MinorBody, 1.0e-12, 1.0e-12, 2.0e-6, 0.0, 0.018,
        {0.35, -1.05, 0.0}, {5.9, 2.8, 0.0},
    }).value);
}

void PhyzSimulation::build_chaos_survival() {
    simulation_.set_fixed_step(0.0001);
    deltaVBudget_ = 2.0;
    missionDeadline_ = 1.4;
    add_gravity(0.006);
    primaryBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Alpha", BodyKind::Star, 1.2, 1.2, 0.0054, 0.0, 0.07,
        {-0.8, 0.0, 0.1}, {0.0, -2.1, 0.1},
    }).value);
    simulation_.add_body({"Beta", BodyKind::Star, 0.9, 0.9, 0.0043, 0.0, 0.065,
                          {0.9, 0.0, -0.1}, {0.0, 2.5, -0.1}});
    targetBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Gamma", BodyKind::Star, 0.45, 0.45, 0.0025, 0.0, 0.05,
        {0.0, 1.8, 0.25}, {-3.1, 0.0, 0.0},
    }).value);
    playerBodyId_ = static_cast<std::int64_t>(simulation_.add_body({
        "Observer", BodyKind::Spacecraft, 0.0, 0.0, 8.0e-8, 0.0, 0.022,
        {0.0, -2.4, 0.5}, {3.4, 0.2, 0.0},
    }).value);
}

Dictionary PhyzSimulation::advance(double years) {
    const double clamped = std::clamp(years, -0.05, 0.05);
    const double requestedTime = simulation_.time() + clamped;
    phyz::engine::AdvanceReport report;
    report.requestedEndTime = requestedTime;
    bool burnExecuted = false;
    if (clamped > 0.0 && scheduledImpulse_ &&
        scheduledImpulse_->time >= simulation_.time() - 1.0e-12 &&
        scheduledImpulse_->time <= requestedTime + 1.0e-12) {
        const auto beforeBurn = simulation_.advance_to(std::max(simulation_.time(), scheduledImpulse_->time));
        report = beforeBurn;
        report.requestedEndTime = requestedTime;
        if (beforeBurn.status == phyz::engine::StepStatus::Success) {
            const auto burn = *scheduledImpulse_;
            scheduledImpulse_.reset();
            burnExecuted = apply_impulse(static_cast<std::int64_t>(burn.body.value),
                                         burn.deltaVelocity.x, burn.deltaVelocity.y, burn.deltaVelocity.z);
            if (burnExecuted && simulation_.time() < requestedTime - 1.0e-15) {
                const auto afterBurn = simulation_.advance_to(requestedTime);
                report.status = afterBurn.status;
                report.reachedTime = afterBurn.reachedTime;
                report.macroSteps += afterBurn.macroSteps;
                report.forceEvaluations += afterBurn.forceEvaluations;
                report.events.insert(report.events.end(), afterBurn.events.begin(), afterBurn.events.end());
            }
        }
    } else {
        report = simulation_.advance_to(requestedTime);
    }
    Dictionary result;
    result["status"] = static_cast<int>(report.status);
    result["time"] = report.reachedTime;
    result["force_evaluations"] = static_cast<std::int64_t>(report.forceEvaluations);
    result["burn_executed"] = burnExecuted;
    Array events;
    for (const auto& event : report.events) {
        events.append(event_dictionary(event));
    }
    result["events"] = events;
    return result;
}

Dictionary PhyzSimulation::advance_controlled(
    double years, double x, double y, double z, double throttle) {
    const double requested = std::clamp(years, 0.0, 0.05);
    const Vec3d command{x, y, z};
    const double commandLength = phyz::engine::length(command);
    const double clampedThrottle = std::clamp(throttle, 0.0, 1.0);
    const double reserved = scheduledImpulse_
        ? phyz::engine::length(scheduledImpulse_->deltaVelocity)
        : 0.0;
    const double remainingBudget = std::max(0.0, deltaVBudget_ - deltaVSpent_ - reserved);
    const double accelerationMagnitude = commandLength > 1.0e-12
        ? maxThrustAcceleration_ * clampedThrottle
        : 0.0;
    const double poweredDuration = accelerationMagnitude > 0.0
        ? std::min(requested, remainingBudget / accelerationMagnitude)
        : 0.0;

    Dictionary result;
    bool burnExecuted = false;
    double consumed = 0.0;
    if (poweredDuration > 0.0 && thrustForce_) {
        const Vec3d acceleration = command * (accelerationMagnitude / commandLength);
        thrustForce_->set_acceleration(acceleration);
        const double before = simulation_.time();
        const double plannedConsumption = accelerationMagnitude * poweredDuration;
        deltaVSpent_ += plannedConsumption;
        result = advance(poweredDuration);
        const double elapsed = std::max(0.0, simulation_.time() - before);
        consumed = accelerationMagnitude * elapsed;
        deltaVSpent_ -= plannedConsumption - consumed;
        burnExecuted = static_cast<bool>(result.get("burn_executed", false));
        thrustForce_->set_acceleration({});
    }
    const double coastDuration = requested - poweredDuration;
    if (coastDuration > 1.0e-15) {
        Dictionary coast = advance(coastDuration);
        burnExecuted = burnExecuted || static_cast<bool>(coast.get("burn_executed", false));
        result = coast;
    } else if (result.is_empty()) {
        result = advance(requested);
    }
    result["burn_executed"] = burnExecuted;
    result["thrust_delta_v"] = consumed;
    result["throttle"] = clampedThrottle;
    result["fuel_depleted"] = accelerationMagnitude > 0.0 && poweredDuration + 1.0e-15 < requested;
    return result;
}

bool PhyzSimulation::apply_impulse(std::int64_t bodyId, double x, double y, double z) {
    const Vec3d delta{x, y, z};
    const double magnitude = phyz::engine::length(delta);
    const double reserved = scheduledImpulse_ ? phyz::engine::length(scheduledImpulse_->deltaVelocity) : 0.0;
    if (!std::isfinite(magnitude) || deltaVSpent_ + reserved + magnitude > deltaVBudget_ + 1.0e-12) {
        return false;
    }
    auto* body = simulation_.find_body(BodyId{static_cast<std::uint64_t>(bodyId)});
    if (!body) {
        return false;
    }
    body->velocity += delta;
    deltaVSpent_ += magnitude;
    return true;
}

bool PhyzSimulation::schedule_impulse(std::int64_t bodyId, double time, double x, double y, double z) {
    const Vec3d delta{x, y, z};
    const double magnitude = phyz::engine::length(delta);
    if (!std::isfinite(time) || time < simulation_.time() - 1.0e-12 ||
        time > missionDeadline_ + 1.0e-12 || !std::isfinite(magnitude) ||
        deltaVSpent_ + magnitude > deltaVBudget_ + 1.0e-12 ||
        !simulation_.find_body(BodyId{static_cast<std::uint64_t>(bodyId)})) {
        return false;
    }
    scheduledImpulse_ = phyz::engine::ImpulseManeuver{
        time, BodyId{static_cast<std::uint64_t>(bodyId)}, delta};
    return true;
}

bool PhyzSimulation::cancel_scheduled_impulse() {
    const bool hadImpulse = scheduledImpulse_.has_value();
    scheduledImpulse_.reset();
    return hadImpulse;
}

Dictionary PhyzSimulation::get_scheduled_impulse() const {
    Dictionary result;
    result["active"] = scheduledImpulse_.has_value();
    if (scheduledImpulse_) {
        result["body_id"] = static_cast<std::int64_t>(scheduledImpulse_->body.value);
        result["time"] = scheduledImpulse_->time;
        result["delta_velocity"] = packed(scheduledImpulse_->deltaVelocity);
    }
    return result;
}

Array PhyzSimulation::predict(std::int64_t bodyId, double x, double y, double z, double burnTime, double duration, double samplePeriod) const {
    Array result;
    const BodyId id{static_cast<std::uint64_t>(bodyId)};
    const double start = simulation_.time();
    const double maneuverTime = std::clamp(burnTime, start, start + std::clamp(duration, 0.0, 5.0));
    const auto prediction = phyz::engine::predict_trajectory(
        simulation_, start + std::clamp(duration, 0.0, 5.0),
        std::clamp(samplePeriod, 1.0e-4, 0.1), {{maneuverTime, id, {x, y, z}}});
    for (const auto& trajectory : prediction.trajectories) {
        if (trajectory.body != id) {
            continue;
        }
        for (const auto& point : trajectory.points) {
            Dictionary item;
            item["time"] = point.time;
            item["position"] = packed(point.position);
            item["velocity"] = packed(point.velocity);
            result.append(item);
        }
    }
    return result;
}

Array PhyzSimulation::get_bodies() const {
    Array result;
    for (const auto& body : simulation_.bodies()) {
        Dictionary item;
        item["id"] = static_cast<std::int64_t>(body.id.value);
        item["mass"] = body.gravitationalMass;
        item["physical_radius"] = body.physicalRadius;
        item["position"] = packed(body.position);
        item["velocity"] = packed(body.velocity);
        item["acceleration"] = packed(body.acceleration);
        if (const auto* meta = simulation_.metadata(body.id)) {
            item["name"] = String(meta->name.c_str());
            item["kind"] = static_cast<int>(meta->kind);
            item["display_radius"] = meta->displayRadius;
        }
        result.append(item);
    }
    return result;
}

Dictionary PhyzSimulation::get_invariants() const {
    const auto report = simulation_.invariants();
    Dictionary result;
    result["kinetic_energy"] = report.kineticEnergy;
    result["has_mechanical_energy"] = report.mechanicalEnergy.has_value();
    result["mechanical_energy"] = report.mechanicalEnergy.value_or(0.0);
    result["linear_momentum"] = packed(report.linearMomentum);
    result["angular_momentum"] = packed(report.angularMomentum);
    result["center_of_mass"] = packed(report.centerOfMass);
    result["momentum_residual"] = report.momentumResidual;
    return result;
}

Dictionary PhyzSimulation::get_orbital_elements(std::int64_t bodyId, std::int64_t primaryId) const {
    Dictionary result;
    const auto* body = simulation_.find_body(BodyId{static_cast<std::uint64_t>(bodyId)});
    const auto* primary = simulation_.find_body(BodyId{static_cast<std::uint64_t>(primaryId)});
    if (!body || !primary) {
        result["valid"] = false;
        return result;
    }
    const double mu = simulation_.units().gravitationalConstant *
        (body->gravitationalMass + primary->gravitationalMass);
    const auto elements = phyz::engine::cartesian_to_orbital_elements(
        body->position - primary->position, body->velocity - primary->velocity, mu);
    result["valid"] = elements.has_value();
    if (elements) {
        result["semi_major_axis"] = elements->semiMajorAxis;
        result["eccentricity"] = elements->eccentricity;
        result["inclination"] = elements->inclination;
        result["periapsis"] = elements->periapsisDistance;
        result["apoapsis"] = elements->apoapsisDistance;
        result["conic"] = static_cast<int>(elements->conic);
    }
    return result;
}

Dictionary PhyzSimulation::get_mission_definition() const {
    static constexpr const char* names[] = {
        "Hohmann Window", "Silent Rendezvous", "Giant's Gift", "Deflection", "Chaos Watch",
    };
    static constexpr const char* objectives[] = {
        "Reach Aster Station with limited delta-v.",
        "Match position and velocity with the drifting probe.",
        "Use the moving giant planet to leave faster than you arrived.",
        "Deflect the asteroid so Haven remains safe at the deadline.",
        "Keep the observer alive inside a chaotic triple system.",
    };
    Dictionary result;
    result["index"] = missionIndex_;
    result["name"] = names[missionIndex_];
    result["objective"] = objectives[missionIndex_];
    result["player_body_id"] = playerBodyId_;
    result["target_body_id"] = targetBodyId_;
    result["primary_body_id"] = primaryBodyId_;
    result["delta_v_budget"] = deltaVBudget_;
    result["delta_v_spent"] = deltaVSpent_;
    result["delta_v_reserved"] = scheduledImpulse_ ? phyz::engine::length(scheduledImpulse_->deltaVelocity) : 0.0;
    result["deadline"] = missionDeadline_;
    result["max_thrust_acceleration"] = maxThrustAcceleration_;
    return result;
}

Dictionary PhyzSimulation::evaluate_mission() const {
    Dictionary result;
    const auto* player = simulation_.find_body(BodyId{static_cast<std::uint64_t>(playerBodyId_)});
    const auto* target = simulation_.find_body(BodyId{static_cast<std::uint64_t>(targetBodyId_)});
    bool success = false;
    bool failed = simulation_.time() > missionDeadline_ + 1.0e-12;
    double metric = 0.0;
    double distance = 0.0;
    double relativeSpeed = 0.0;
    double speedRatio = 0.0;
    if (player && target) {
        distance = phyz::engine::length(player->position - target->position);
        relativeSpeed = phyz::engine::length(player->velocity - target->velocity);
        speedRatio = initialPlayerSpeed_ > 0.0
            ? phyz::engine::length(player->velocity) / initialPlayerSpeed_
            : 0.0;
        metric = distance;
        switch (missionIndex_) {
        case 0: success = distance < 0.12; break;
        case 1: success = distance < 0.035 && relativeSpeed < 0.65; break;
        case 2:
            success = simulation_.time() > 0.18 && speedRatio > 0.96;
            metric = speedRatio;
            break;
        case 3: success = simulation_.time() >= missionDeadline_ && distance > 0.25; failed = simulation_.time() >= missionDeadline_ && !success; break;
        case 4: success = simulation_.time() >= missionDeadline_; failed = !player || simulation_.time() > missionDeadline_ + 0.05; break;
        }
    } else {
        failed = true;
    }
    const double fuelScore = deltaVBudget_ > 0.0 ? std::max(0.0, 1.0 - deltaVSpent_ / deltaVBudget_) : 1.0;
    result["success"] = success;
    result["failed"] = failed && !success;
    result["metric"] = metric;
    result["distance"] = distance;
    result["relative_speed"] = relativeSpeed;
    result["speed_ratio"] = speedRatio;
    result["time_remaining"] = std::max(0.0, missionDeadline_ - simulation_.time());
    result["score"] = success ? std::round((600.0 + 400.0 * fuelScore) * 10.0) / 10.0 : 0.0;
    result["delta_v_remaining"] = std::max(0.0, deltaVBudget_ - deltaVSpent_);
    return result;
}

String PhyzSimulation::save_snapshot() const {
    std::ostringstream output;
    output << std::setprecision(17);
    output << "PHYZBOX_GAME_SNAPSHOT 1\n";
    output << missionIndex_ << ' ' << playerBodyId_ << ' ' << targetBodyId_ << ' '
           << primaryBodyId_ << ' ' << deltaVBudget_ << ' ' << deltaVSpent_ << ' '
           << missionDeadline_ << ' ' << initialPlayerSpeed_ << '\n';
    output << (scheduledImpulse_ ? 1 : 0);
    if (scheduledImpulse_) {
        output << ' ' << scheduledImpulse_->time << ' ' << scheduledImpulse_->body.value << ' '
               << scheduledImpulse_->deltaVelocity.x << ' ' << scheduledImpulse_->deltaVelocity.y << ' '
               << scheduledImpulse_->deltaVelocity.z;
    }
    output << '\n' << simulation_.serialize_snapshot();
    return String(output.str().c_str());
}

bool PhyzSimulation::load_snapshot(const String& text) {
    const CharString utf8 = text.utf8();
    const std::string snapshotText(utf8.get_data(), static_cast<std::size_t>(utf8.length()));
    std::istringstream input(snapshotText);
    std::string marker;
    int version = 0;
    if (!(input >> marker >> version) || marker != "PHYZBOX_GAME_SNAPSHOT" || version != 1) {
        return false;
    }
    int missionIndex = 0;
    std::int64_t playerBodyId = 0;
    std::int64_t targetBodyId = 0;
    std::int64_t primaryBodyId = 0;
    double deltaVBudget = 0.0;
    double deltaVSpent = 0.0;
    double missionDeadline = 0.0;
    double initialPlayerSpeed = 0.0;
    int hasScheduled = 0;
    if (!(input >> missionIndex >> playerBodyId >> targetBodyId >> primaryBodyId >>
          deltaVBudget >> deltaVSpent >> missionDeadline >> initialPlayerSpeed >> hasScheduled)) {
        return false;
    }
    std::optional<phyz::engine::ImpulseManeuver> scheduledImpulse;
    if (hasScheduled) {
        phyz::engine::ImpulseManeuver maneuver;
        if (!(input >> maneuver.time >> maneuver.body.value >> maneuver.deltaVelocity.x >>
              maneuver.deltaVelocity.y >> maneuver.deltaVelocity.z)) {
            return false;
        }
        scheduledImpulse = maneuver;
    }
    std::string line;
    std::getline(input, line);
    const std::streampos physicsStart = input.tellg();
    if (physicsStart < 0) {
        return false;
    }
    auto restored = phyz::engine::Simulation::deserialize_snapshot(
        std::string_view(snapshotText).substr(static_cast<std::size_t>(physicsStart)));
    if (!restored) {
        return false;
    }
    if (missionIndex < 0 || missionIndex > 4 || playerBodyId <= 0 || targetBodyId <= 0 ||
        primaryBodyId <= 0 || !std::isfinite(deltaVBudget) || !std::isfinite(deltaVSpent) ||
        !std::isfinite(missionDeadline) || !std::isfinite(initialPlayerSpeed) ||
        deltaVBudget < 0.0 || deltaVSpent < 0.0 || deltaVSpent > deltaVBudget + 1.0e-12) {
        return false;
    }
    if (scheduledImpulse) {
        const double magnitude = phyz::engine::length(scheduledImpulse->deltaVelocity);
        if (!std::isfinite(scheduledImpulse->time) || !std::isfinite(magnitude) ||
            scheduledImpulse->time < restored->time() - 1.0e-12 ||
            scheduledImpulse->time > missionDeadline + 1.0e-12 ||
            deltaVSpent + magnitude > deltaVBudget + 1.0e-12 ||
            !restored->find_body(scheduledImpulse->body)) {
            return false;
        }
    }
    simulation_ = std::move(*restored);
    missionIndex_ = missionIndex;
    playerBodyId_ = playerBodyId;
    targetBodyId_ = targetBodyId;
    primaryBodyId_ = primaryBodyId;
    deltaVBudget_ = deltaVBudget;
    deltaVSpent_ = deltaVSpent;
    missionDeadline_ = missionDeadline;
    initialPlayerSpeed_ = initialPlayerSpeed;
    scheduledImpulse_ = scheduledImpulse;
    attach_thrust_force();
    return true;
}

double PhyzSimulation::get_time() const { return simulation_.time(); }
double PhyzSimulation::get_fixed_step() const { return simulation_.fixed_step(); }
String PhyzSimulation::get_integrator_name() const { return String(simulation_.integrator().name().data()); }

BodyKind PhyzSimulation::kind_from_int(int value) {
    return static_cast<BodyKind>(std::clamp(value, 0, static_cast<int>(BodyKind::Spacecraft)));
}

} // namespace godot
