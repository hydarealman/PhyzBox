#include "phyz/libphyz.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

namespace phyz::engine {
namespace {

constexpr double Epsilon = 1.0e-15;

bool finite(const Vec3d& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

double clamp_unit(double value) {
    return std::clamp(value, -1.0, 1.0);
}

double wrapped_angle(double value) {
    const double twoPi = 2.0 * std::numbers::pi;
    value = std::fmod(value, twoPi);
    return value < 0.0 ? value + twoPi : value;
}

double pair_scale(const BodyState& a, const BodyState& b, double distance, const PairwiseGravity& model) {
    if (model.law() == GravityLaw::PaczynskiWiita) {
        const double horizon = std::max(a.schwarzschildRadius, b.schwarzschildRadius);
        if (horizon > 0.0) {
            const double effective = std::max(distance - horizon, horizon * 0.25);
            return model.gravitational_constant() / (distance * effective * effective);
        }
    }
    if (model.law() == GravityLaw::PlummerSoftened || model.softening() > 0.0) {
        const double softened2 = distance * distance + model.softening() * model.softening();
        const double inverse = 1.0 / std::sqrt(softened2);
        return model.gravitational_constant() * inverse * inverse * inverse;
    }
    return model.gravitational_constant() / (distance * distance * distance);
}

double pair_potential(const BodyState& a, const BodyState& b, double distance, const PairwiseGravity& model) {
    if (model.law() == GravityLaw::PaczynskiWiita) {
        const double horizon = std::max(a.schwarzschildRadius, b.schwarzschildRadius);
        if (horizon > 0.0) {
            const double effective = std::max(distance - horizon, horizon * 0.25);
            return -model.gravitational_constant() * a.gravitationalMass * b.gravitationalMass / effective;
        }
    }
    if (model.law() == GravityLaw::PlummerSoftened || model.softening() > 0.0) {
        distance = std::sqrt(distance * distance + model.softening() * model.softening());
    }
    return -model.gravitational_constant() * a.gravitationalMass * b.gravitationalMass / distance;
}

void leapfrog_substep(
    std::span<BodyState> bodies,
    const ForcePipeline& forces,
    double time,
    double dt,
    std::vector<Vec3d>& accelerations,
    std::size_t& evaluations) {
    accelerations.resize(bodies.size());
    forces.calculate(bodies, time, accelerations);
    ++evaluations;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        bodies[i].velocity += accelerations[i] * (0.5 * dt);
        bodies[i].position += bodies[i].velocity * dt;
    }
    forces.calculate(bodies, time + dt, accelerations);
    ++evaluations;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        bodies[i].velocity += accelerations[i] * (0.5 * dt);
        bodies[i].acceleration = accelerations[i];
    }
}

bool finite_state(std::span<const BodyState> bodies) {
    for (const BodyState& body : bodies) {
        if (!std::isfinite(body.gravitationalMass) || !std::isfinite(body.inertialMass) ||
            !finite(body.position) || !finite(body.velocity) || !finite(body.acceleration)) {
            return false;
        }
    }
    return true;
}

double segment_minimum_distance(Vec3d start, Vec3d end, double& fraction) {
    const Vec3d delta = end - start;
    const double denominator = length_squared(delta);
    fraction = denominator > Epsilon ? std::clamp(-dot(start, delta) / denominator, 0.0, 1.0) : 0.0;
    return length(start + delta * fraction);
}

double segment_threshold_entry(Vec3d start, Vec3d end, double threshold) {
    const Vec3d delta = end - start;
    const double a = length_squared(delta);
    if (a <= Epsilon) {
        return 0.0;
    }
    const double b = 2.0 * dot(start, delta);
    const double c = length_squared(start) - threshold * threshold;
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) {
        return std::clamp(-dot(start, delta) / a, 0.0, 1.0);
    }
    const double root = (-b - std::sqrt(discriminant)) / (2.0 * a);
    return std::clamp(root, 0.0, 1.0);
}

} // namespace

ForcePipeline::ForcePipeline(const ForcePipeline& other) {
    for (const auto& model : other.models_) {
        models_.push_back(model->clone());
    }
}

ForcePipeline& ForcePipeline::operator=(const ForcePipeline& other) {
    if (this == &other) {
        return *this;
    }
    models_.clear();
    for (const auto& model : other.models_) {
        models_.push_back(model->clone());
    }
    return *this;
}

void ForcePipeline::add(std::unique_ptr<ForceModel> model) {
    if (model) {
        models_.push_back(std::move(model));
    }
}

void ForcePipeline::clear() {
    models_.clear();
}

void ForcePipeline::calculate(std::span<const BodyState> bodies, double time, std::span<Vec3d> output) const {
    if (output.size() != bodies.size()) {
        throw std::invalid_argument("acceleration buffer size must match body count");
    }
    std::fill(output.begin(), output.end(), Vec3d{});
    for (const auto& model : models_) {
        model->accumulate(bodies, time, output);
    }
}

std::optional<double> ForcePipeline::potential_energy(std::span<const BodyState> bodies, double time) const {
    double total = 0.0;
    for (const auto& model : models_) {
        const std::optional<double> contribution = model->potential_energy(bodies, time);
        if (!contribution) {
            return std::nullopt;
        }
        total += *contribution;
    }
    return total;
}

bool ForcePipeline::is_conservative() const {
    return std::all_of(models_.begin(), models_.end(), [](const auto& model) {
        return model->traits().conservative;
    });
}

bool ForcePipeline::is_velocity_dependent() const {
    return std::any_of(models_.begin(), models_.end(), [](const auto& model) {
        return model->traits().velocityDependent;
    });
}

PairwiseGravity::PairwiseGravity(double gravitationalConstant, GravityLaw law, double softening)
    : gravitationalConstant_(gravitationalConstant), law_(law), softening_(std::max(0.0, softening)) {
    if (!(gravitationalConstant_ > 0.0) || !std::isfinite(gravitationalConstant_)) {
        throw std::invalid_argument("gravitational constant must be positive and finite");
    }
}

std::string_view PairwiseGravity::name() const noexcept {
    switch (law_) {
    case GravityLaw::Newtonian: return "Newtonian gravity";
    case GravityLaw::PlummerSoftened: return "Plummer-softened gravity";
    case GravityLaw::PaczynskiWiita: return "Paczynski-Wiita gravity";
    }
    return "pairwise gravity";
}

std::unique_ptr<ForceModel> PairwiseGravity::clone() const {
    return std::make_unique<PairwiseGravity>(*this);
}

void PairwiseGravity::accumulate(
    std::span<const BodyState> bodies,
    double,
    std::span<Vec3d> accelerations) const {
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const Vec3d delta = bodies[j].position - bodies[i].position;
            const double distance = length(delta);
            if (distance <= Epsilon) {
                continue;
            }
            const double scale = pair_scale(bodies[i], bodies[j], distance, *this);
            accelerations[i] += delta * (scale * bodies[j].gravitationalMass);
            accelerations[j] -= delta * (scale * bodies[i].gravitationalMass);
        }
    }
}

std::optional<double> PairwiseGravity::potential_energy(std::span<const BodyState> bodies, double) const {
    double result = 0.0;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const double distance = length(bodies[j].position - bodies[i].position);
            if (distance > Epsilon) {
                result += pair_potential(bodies[i], bodies[j], distance, *this);
            }
        }
    }
    return result;
}

ConstantAcceleration::ConstantAcceleration(BodyId target, Vec3d acceleration)
    : target_(target), acceleration_(acceleration) {}

std::unique_ptr<ForceModel> ConstantAcceleration::clone() const {
    return std::make_unique<ConstantAcceleration>(*this);
}

void ConstantAcceleration::accumulate(
    std::span<const BodyState> bodies,
    double,
    std::span<Vec3d> accelerations) const {
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].id == target_) {
            accelerations[i] += acceleration_;
            return;
        }
    }
}

std::unique_ptr<Integrator> Leapfrog2Fixed::clone() const {
    return std::make_unique<Leapfrog2Fixed>();
}

StepResult Leapfrog2Fixed::step(
    std::span<BodyState> bodies,
    const ForcePipeline& forces,
    double time,
    double dt) {
    if (!std::isfinite(dt) || dt == 0.0) {
        return {StepStatus::InvalidTimeStep};
    }
    if (forces.is_velocity_dependent()) {
        return {StepStatus::IncompatibleForce};
    }
    std::size_t evaluations = 0;
    leapfrog_substep(bodies, forces, time, dt, accelerations_, evaluations);
    return {
        finite_state(bodies) ? StepStatus::Success : StepStatus::NonFiniteState,
        dt,
        std::abs(dt),
        evaluations,
        1,
        0,
        0.0,
    };
}

std::unique_ptr<Integrator> Yoshida4Fixed::clone() const {
    return std::make_unique<Yoshida4Fixed>();
}

StepResult Yoshida4Fixed::step(
    std::span<BodyState> bodies,
    const ForcePipeline& forces,
    double time,
    double dt) {
    if (!std::isfinite(dt) || dt == 0.0) {
        return {StepStatus::InvalidTimeStep};
    }
    if (forces.is_velocity_dependent()) {
        return {StepStatus::IncompatibleForce};
    }
    const double cubeRootTwo = std::cbrt(2.0);
    const double w1 = 1.0 / (2.0 - cubeRootTwo);
    const double w0 = -cubeRootTwo / (2.0 - cubeRootTwo);
    StepResult result;
    const StepResult first = leapfrog_.step(bodies, forces, time, w1 * dt);
    const StepResult second = leapfrog_.step(bodies, forces, time + w1 * dt, w0 * dt);
    const StepResult third = leapfrog_.step(bodies, forces, time + (w1 + w0) * dt, w1 * dt);
    result.status = first.status != StepStatus::Success ? first.status :
        (second.status != StepStatus::Success ? second.status : third.status);
    result.advancedTime = dt;
    result.smallestStep = std::min({std::abs(w1 * dt), std::abs(w0 * dt), std::abs(w1 * dt)});
    result.forceEvaluations = first.forceEvaluations + second.forceEvaluations + third.forceEvaluations;
    result.acceptedSteps = 1;
    return result;
}

InvariantReport calculate_invariants(
    std::span<const BodyState> bodies,
    const ForcePipeline& forces,
    double time) {
    InvariantReport report;
    double momentumScale = 0.0;
    for (const BodyState& body : bodies) {
        report.kineticEnergy += 0.5 * body.inertialMass * length_squared(body.velocity);
        report.linearMomentum += body.velocity * body.inertialMass;
        report.angularMomentum += cross(body.position, body.velocity) * body.inertialMass;
        report.centerOfMass += body.position * body.inertialMass;
        report.centerOfMassVelocity += body.velocity * body.inertialMass;
        report.totalInertialMass += body.inertialMass;
        momentumScale += std::abs(body.inertialMass) * length(body.velocity);
    }
    if (report.totalInertialMass > Epsilon) {
        report.centerOfMass = report.centerOfMass / report.totalInertialMass;
        report.centerOfMassVelocity = report.centerOfMassVelocity / report.totalInertialMass;
    }
    report.potentialEnergy = forces.potential_energy(bodies, time);
    if (report.potentialEnergy) {
        report.mechanicalEnergy = report.kineticEnergy + *report.potentialEnergy;
    }
    report.momentumResidual = momentumScale > Epsilon ? length(report.linearMomentum) / momentumScale : length(report.linearMomentum);
    return report;
}

std::optional<OrbitalElements> cartesian_to_orbital_elements(
    Vec3d r,
    Vec3d v,
    double mu) {
    const double radius = length(r);
    if (!(mu > 0.0) || radius <= Epsilon || !finite(r) || !finite(v)) {
        return std::nullopt;
    }
    OrbitalElements elements;
    const double speed2 = length_squared(v);
    const Vec3d h = cross(r, v);
    const double hMagnitude = length(h);
    if (hMagnitude <= Epsilon) {
        return std::nullopt;
    }
    const Vec3d node = cross({0.0, 0.0, 1.0}, h);
    const double nodeMagnitude = length(node);
    const Vec3d eccentricityVector = cross(v, h) / mu - r / radius;
    const double eccentricity = length(eccentricityVector);
    const double energy = 0.5 * speed2 - mu / radius;

    elements.specificAngularMomentum = h;
    elements.eccentricityVector = eccentricityVector;
    elements.eccentricity = eccentricity;
    elements.specificOrbitalEnergy = energy;
    elements.inclination = std::acos(clamp_unit(h.z / hMagnitude));
    elements.periapsisDistance = hMagnitude * hMagnitude / (mu * (1.0 + eccentricity));

    if (std::abs(eccentricity - 1.0) < 1.0e-9) {
        elements.conic = ConicType::Parabolic;
        elements.semiMajorAxis = std::numeric_limits<double>::infinity();
        elements.apoapsisDistance = std::numeric_limits<double>::infinity();
    } else {
        elements.semiMajorAxis = -mu / (2.0 * energy);
        elements.apoapsisDistance = eccentricity < 1.0
            ? elements.semiMajorAxis * (1.0 + eccentricity)
            : std::numeric_limits<double>::infinity();
        elements.conic = eccentricity < 1.0e-9 ? ConicType::Circular :
            (eccentricity < 1.0 ? ConicType::Elliptic : ConicType::Hyperbolic);
    }

    if (nodeMagnitude > Epsilon) {
        elements.longitudeOfAscendingNode = wrapped_angle(std::atan2(node.y, node.x));
    }
    if (eccentricity > 1.0e-9 && nodeMagnitude > Epsilon) {
        const double cosine = clamp_unit(dot(node, eccentricityVector) / (nodeMagnitude * eccentricity));
        elements.argumentOfPeriapsis = std::acos(cosine);
        if (eccentricityVector.z < 0.0) {
            elements.argumentOfPeriapsis = 2.0 * std::numbers::pi - elements.argumentOfPeriapsis;
        }
    }
    if (eccentricity > 1.0e-9) {
        const double cosine = clamp_unit(dot(eccentricityVector, r) / (eccentricity * radius));
        elements.trueAnomaly = std::acos(cosine);
        if (dot(r, v) < 0.0) {
            elements.trueAnomaly = 2.0 * std::numbers::pi - elements.trueAnomaly;
        }
    } else if (nodeMagnitude > Epsilon) {
        elements.trueAnomaly = std::acos(clamp_unit(dot(node, r) / (nodeMagnitude * radius)));
        if (r.z < 0.0) {
            elements.trueAnomaly = 2.0 * std::numbers::pi - elements.trueAnomaly;
        }
    } else {
        elements.trueAnomaly = wrapped_angle(std::atan2(r.y, r.x));
    }
    return elements;
}

std::unique_ptr<EventDetector> CollisionDetector::clone() const {
    return std::make_unique<CollisionDetector>(*this);
}

void CollisionDetector::detect(
    std::span<const BodyState> previous,
    std::span<const BodyState> current,
    double startTime,
    double endTime,
    std::vector<Event>& output) const {
    const std::size_t count = std::min(previous.size(), current.size());
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            const double threshold = current[i].physicalRadius + current[j].physicalRadius;
            const Vec3d before = previous[j].position - previous[i].position;
            const Vec3d after = current[j].position - current[i].position;
            double fraction = 0.0;
            const double minimum = segment_minimum_distance(before, after, fraction);
            if (length(before) > threshold && minimum <= threshold) {
                fraction = segment_threshold_entry(before, after, threshold);
                output.push_back({
                    EventType::Collision,
                    startTime + (endTime - startTime) * fraction,
                    current[i].id,
                    current[j].id,
                    minimum,
                    threshold,
                });
            }
        }
    }
}

std::unique_ptr<EventDetector> CloseApproachDetector::clone() const {
    return std::make_unique<CloseApproachDetector>(*this);
}

void CloseApproachDetector::detect(
    std::span<const BodyState> previous,
    std::span<const BodyState> current,
    double startTime,
    double endTime,
    std::vector<Event>& output) const {
    const std::size_t count = std::min(previous.size(), current.size());
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            const Vec3d before = previous[j].position - previous[i].position;
            const Vec3d after = current[j].position - current[i].position;
            double fraction = 0.0;
            const double minimum = segment_minimum_distance(before, after, fraction);
            if (length(before) > threshold_ && minimum <= threshold_) {
                fraction = segment_threshold_entry(before, after, threshold_);
                output.push_back({
                    EventType::CloseApproach,
                    startTime + (endTime - startTime) * fraction,
                    current[i].id,
                    current[j].id,
                    minimum,
                    threshold_,
                });
            }
        }
    }
}

std::unique_ptr<EventDetector> EventHorizonDetector::clone() const {
    return std::make_unique<EventHorizonDetector>(*this);
}

void EventHorizonDetector::detect(
    std::span<const BodyState> previous,
    std::span<const BodyState> current,
    double startTime,
    double endTime,
    std::vector<Event>& output) const {
    const std::size_t count = std::min(previous.size(), current.size());
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            const double threshold = std::max(current[i].schwarzschildRadius, current[j].schwarzschildRadius);
            if (threshold <= 0.0) {
                continue;
            }
            const Vec3d before = previous[j].position - previous[i].position;
            const Vec3d after = current[j].position - current[i].position;
            double fraction = 0.0;
            const double minimum = segment_minimum_distance(before, after, fraction);
            if (length(before) > threshold && minimum <= threshold) {
                fraction = segment_threshold_entry(before, after, threshold);
                output.push_back({EventType::EventHorizonCrossing,
                                  startTime + (endTime - startTime) * fraction,
                                  current[i].id, current[j].id, minimum, threshold});
            }
        }
    }
}

std::unique_ptr<EventDetector> RocheLimitDetector::clone() const {
    return std::make_unique<RocheLimitDetector>(*this);
}

void RocheLimitDetector::detect(
    std::span<const BodyState> previous,
    std::span<const BodyState> current,
    double startTime,
    double endTime,
    std::vector<Event>& output) const {
    const std::size_t count = std::min(previous.size(), current.size());
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            const std::size_t primaryIndex = current[i].gravitationalMass >= current[j].gravitationalMass ? i : j;
            const std::size_t secondaryIndex = primaryIndex == i ? j : i;
            const BodyState& primary = current[primaryIndex];
            const BodyState& secondary = current[secondaryIndex];
            if (primary.gravitationalMass <= 0.0 || secondary.gravitationalMass <= 0.0 ||
                primary.physicalRadius <= 0.0 || secondary.physicalRadius <= 0.0) {
                continue;
            }
            const double primaryDensity = primary.gravitationalMass /
                (primary.physicalRadius * primary.physicalRadius * primary.physicalRadius);
            const double secondaryDensity = secondary.gravitationalMass /
                (secondary.physicalRadius * secondary.physicalRadius * secondary.physicalRadius);
            const double threshold = primary.schwarzschildRadius > 0.0
                ? secondary.physicalRadius * std::cbrt(2.0 * primary.gravitationalMass / secondary.gravitationalMass)
                : 2.44 * primary.physicalRadius * std::cbrt(primaryDensity / secondaryDensity);
            const Vec3d before = previous[secondaryIndex].position - previous[primaryIndex].position;
            const Vec3d after = secondary.position - primary.position;
            double fraction = 0.0;
            const double minimum = segment_minimum_distance(before, after, fraction);
            if (length(before) > threshold && minimum <= threshold) {
                fraction = segment_threshold_entry(before, after, threshold);
                output.push_back({EventType::RocheLimitCrossing,
                                  startTime + (endTime - startTime) * fraction,
                                  primary.id, secondary.id, minimum, threshold});
            }
        }
    }
}

Simulation::Simulation(UnitSystem units)
    : units_(units), integrator_(std::make_unique<Yoshida4Fixed>()) {}

Simulation::Simulation(const Simulation& other)
    : units_(other.units_),
      time_(other.time_),
      fixedStep_(other.fixedStep_),
      nextBodyId_(other.nextBodyId_),
      bodies_(other.bodies_),
      metadata_(other.metadata_),
      forces_(other.forces_),
      integrator_(other.integrator_->clone()),
      previousStates_(other.previousStates_) {
    for (const auto& detector : other.detectors_) {
        detectors_.push_back(detector->clone());
    }
}

Simulation& Simulation::operator=(const Simulation& other) {
    if (this == &other) {
        return *this;
    }
    Simulation copy(other);
    *this = std::move(copy);
    return *this;
}

BodyId Simulation::add_body(const BodyDefinition& definition) {
    if (!std::isfinite(definition.gravitationalMass) || definition.gravitationalMass < 0.0 ||
        !std::isfinite(definition.inertialMass) || definition.inertialMass < 0.0 ||
        !finite(definition.position) || !finite(definition.velocity)) {
        throw std::invalid_argument("body definition contains invalid state");
    }
    const BodyId id{nextBodyId_++};
    bodies_.push_back({
        id,
        definition.gravitationalMass,
        definition.inertialMass,
        std::max(0.0, definition.physicalRadius),
        std::max(0.0, definition.schwarzschildRadius),
        definition.position,
        definition.velocity,
        {},
    });
    metadata_.push_back({id, definition.name, definition.kind, definition.displayRadius});
    return id;
}

bool Simulation::remove_body(BodyId id) {
    const auto body = std::find_if(bodies_.begin(), bodies_.end(), [id](const BodyState& value) { return value.id == id; });
    if (body == bodies_.end()) {
        return false;
    }
    bodies_.erase(body);
    const auto meta = std::find_if(metadata_.begin(), metadata_.end(), [id](const BodyMetadata& value) { return value.id == id; });
    if (meta != metadata_.end()) {
        metadata_.erase(meta);
    }
    return true;
}

BodyState* Simulation::find_body(BodyId id) {
    const auto iterator = std::find_if(bodies_.begin(), bodies_.end(), [id](const BodyState& value) { return value.id == id; });
    return iterator == bodies_.end() ? nullptr : &*iterator;
}

const BodyState* Simulation::find_body(BodyId id) const {
    const auto iterator = std::find_if(bodies_.begin(), bodies_.end(), [id](const BodyState& value) { return value.id == id; });
    return iterator == bodies_.end() ? nullptr : &*iterator;
}

const BodyMetadata* Simulation::metadata(BodyId id) const {
    const auto iterator = std::find_if(metadata_.begin(), metadata_.end(), [id](const BodyMetadata& value) { return value.id == id; });
    return iterator == metadata_.end() ? nullptr : &*iterator;
}

void Simulation::add_detector(std::unique_ptr<EventDetector> detector) {
    if (detector) {
        detectors_.push_back(std::move(detector));
    }
}

void Simulation::clear_detectors() {
    detectors_.clear();
}

void Simulation::set_fixed_step(double step) {
    if (!(step > 0.0) || !std::isfinite(step)) {
        throw std::invalid_argument("fixed step must be positive and finite");
    }
    fixedStep_ = step;
}

AdvanceReport Simulation::step(double dt) {
    AdvanceReport report;
    report.requestedEndTime = time_ + dt;
    report.reachedTime = time_;
    if (!integrator_ || dt == 0.0 || !std::isfinite(dt)) {
        report.status = StepStatus::InvalidTimeStep;
        return report;
    }
    previousStates_ = bodies_;
    const StepResult stepResult = integrator_->step(bodies_, forces_, time_, dt);
    report.status = stepResult.status;
    report.forceEvaluations = stepResult.forceEvaluations;
    if (stepResult.status != StepStatus::Success) {
        bodies_ = previousStates_;
        return report;
    }
    const double start = time_;
    time_ += dt;
    report.reachedTime = time_;
    report.macroSteps = 1;
    for (const auto& detector : detectors_) {
        detector->detect(previousStates_, bodies_, start, time_, report.events);
    }
    std::sort(report.events.begin(), report.events.end(), [](const Event& a, const Event& b) { return a.time < b.time; });
    return report;
}

AdvanceReport Simulation::advance_to(double endTime) {
    AdvanceReport total;
    total.requestedEndTime = endTime;
    total.reachedTime = time_;
    if (!std::isfinite(endTime) || endTime == time_) {
        return total;
    }
    const double direction = endTime > time_ ? 1.0 : -1.0;
    while ((endTime - time_) * direction > Epsilon) {
        const double dt = direction * std::min(fixedStep_, std::abs(endTime - time_));
        AdvanceReport current = step(dt);
        total.forceEvaluations += current.forceEvaluations;
        total.macroSteps += current.macroSteps;
        total.events.insert(total.events.end(), current.events.begin(), current.events.end());
        total.status = current.status;
        total.reachedTime = time_;
        if (current.status != StepStatus::Success) {
            break;
        }
    }
    return total;
}

InvariantReport Simulation::invariants() const {
    return calculate_invariants(bodies_, forces_, time_);
}

SimulationSnapshot Simulation::snapshot() const {
    return {Version, units_, time_, fixedStep_, nextBodyId_, bodies_, metadata_};
}

bool Simulation::restore(const SimulationSnapshot& snapshotValue) {
    if (snapshotValue.bodies.size() != snapshotValue.metadata.size() ||
        !std::isfinite(snapshotValue.time) || !(snapshotValue.fixedStep > 0.0)) {
        return false;
    }
    units_ = snapshotValue.units;
    time_ = snapshotValue.time;
    fixedStep_ = snapshotValue.fixedStep;
    nextBodyId_ = snapshotValue.nextBodyId;
    bodies_ = snapshotValue.bodies;
    metadata_ = snapshotValue.metadata;
    return finite_state(bodies_);
}

std::string Simulation::serialize_snapshot() const {
    std::ostringstream output;
    output << std::setprecision(17);
    output << "LIBPHYZ_SNAPSHOT 1\n";
    output << "version " << Version << "\n";
    output << "units " << units_.lengthInMeters << ' ' << units_.massInKilograms << ' '
           << units_.timeInSeconds << ' ' << units_.gravitationalConstant << "\n";
    output << "state " << time_ << ' ' << fixedStep_ << ' ' << nextBodyId_ << "\n";
    output << "integrator " << (dynamic_cast<const Leapfrog2Fixed*>(integrator_.get()) ? "leapfrog2" : "yoshida4") << "\n";
    output << "forces " << forces_.models().size() << "\n";
    for (const auto& force : forces_.models()) {
        if (const auto* gravity = dynamic_cast<const PairwiseGravity*>(force.get())) {
            output << "gravity " << static_cast<int>(gravity->law()) << ' '
                   << gravity->gravitational_constant() << ' ' << gravity->softening() << "\n";
        } else if (const auto* acceleration = dynamic_cast<const ConstantAcceleration*>(force.get())) {
            const Vec3d value = acceleration->acceleration();
            output << "constant " << acceleration->target().value << ' '
                   << value.x << ' ' << value.y << ' ' << value.z << "\n";
        } else {
            output << "unsupported\n";
        }
    }
    output << "bodies " << bodies_.size() << "\n";
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        const BodyState& body = bodies_[i];
        const BodyMetadata& meta = metadata_[i];
        output << "body " << body.id.value << ' ' << static_cast<int>(meta.kind) << ' '
               << body.gravitationalMass << ' ' << body.inertialMass << ' '
               << body.physicalRadius << ' ' << body.schwarzschildRadius << ' ' << meta.displayRadius << ' '
               << body.position.x << ' ' << body.position.y << ' ' << body.position.z << ' '
               << body.velocity.x << ' ' << body.velocity.y << ' ' << body.velocity.z << ' '
               << body.acceleration.x << ' ' << body.acceleration.y << ' ' << body.acceleration.z << ' '
               << std::quoted(meta.name) << "\n";
    }
    return output.str();
}

std::optional<Simulation> Simulation::deserialize_snapshot(std::string_view text, std::string* error) {
    auto fail = [error](const char* message) -> std::optional<Simulation> {
        if (error) {
            *error = message;
        }
        return std::nullopt;
    };
    std::istringstream input{std::string(text)};
    std::string token;
    int format = 0;
    if (!(input >> token >> format) || token != "LIBPHYZ_SNAPSHOT" || format != 1) {
        return fail("unsupported snapshot header");
    }
    std::string version;
    if (!(input >> token >> version) || token != "version") {
        return fail("missing version");
    }
    UnitSystem units;
    if (!(input >> token >> units.lengthInMeters >> units.massInKilograms >> units.timeInSeconds >> units.gravitationalConstant) || token != "units") {
        return fail("invalid units");
    }
    double time = 0.0;
    double stepSize = 0.0;
    std::uint64_t nextId = 1;
    if (!(input >> token >> time >> stepSize >> nextId) || token != "state") {
        return fail("invalid state header");
    }
    std::string integratorName;
    if (!(input >> token >> integratorName) || token != "integrator") {
        return fail("missing integrator");
    }
    Simulation simulation(units);
    simulation.time_ = time;
    simulation.fixedStep_ = stepSize;
    simulation.nextBodyId_ = nextId;
    if (integratorName == "leapfrog2") {
        simulation.set_integrator<Leapfrog2Fixed>();
    } else if (integratorName == "yoshida4") {
        simulation.set_integrator<Yoshida4Fixed>();
    } else {
        return fail("unknown integrator");
    }
    std::size_t forceCount = 0;
    if (!(input >> token >> forceCount) || token != "forces") {
        return fail("invalid force count");
    }
    for (std::size_t i = 0; i < forceCount; ++i) {
        if (!(input >> token)) {
            return fail("missing force record");
        }
        if (token == "gravity") {
            int law = 0;
            double constant = 0.0;
            double softening = 0.0;
            if (!(input >> law >> constant >> softening) || law < 0 || law > 2) {
                return fail("invalid gravity record");
            }
            simulation.forces_.add<PairwiseGravity>(constant, static_cast<GravityLaw>(law), softening);
        } else if (token == "constant") {
            std::uint64_t id = 0;
            Vec3d acceleration;
            if (!(input >> id >> acceleration.x >> acceleration.y >> acceleration.z)) {
                return fail("invalid acceleration record");
            }
            simulation.forces_.add<ConstantAcceleration>(BodyId{id}, acceleration);
        } else {
            return fail("unsupported force record");
        }
    }
    std::size_t bodyCount = 0;
    if (!(input >> token >> bodyCount) || token != "bodies") {
        return fail("invalid body count");
    }
    for (std::size_t i = 0; i < bodyCount; ++i) {
        BodyState body;
        BodyMetadata meta;
        int kind = 0;
        if (!(input >> token >> body.id.value >> kind >> body.gravitationalMass >> body.inertialMass >>
              body.physicalRadius >> body.schwarzschildRadius >> meta.displayRadius >>
              body.position.x >> body.position.y >> body.position.z >>
              body.velocity.x >> body.velocity.y >> body.velocity.z >>
              body.acceleration.x >> body.acceleration.y >> body.acceleration.z >> std::quoted(meta.name)) || token != "body") {
            return fail("invalid body record");
        }
        if (kind < 0 || kind > static_cast<int>(BodyKind::Spacecraft)) {
            return fail("invalid body kind");
        }
        meta.id = body.id;
        meta.kind = static_cast<BodyKind>(kind);
        simulation.bodies_.push_back(body);
        simulation.metadata_.push_back(std::move(meta));
    }
    if (!finite_state(simulation.bodies_)) {
        return fail("snapshot contains non-finite state");
    }
    return simulation;
}

TrajectoryPrediction predict_trajectory(
    const Simulation& source,
    double endTime,
    double samplePeriod,
    std::vector<ImpulseManeuver> maneuvers) {
    TrajectoryPrediction prediction;
    prediction.startTime = source.time();
    prediction.endTime = endTime;
    if (!(samplePeriod > 0.0) || !std::isfinite(samplePeriod) || !std::isfinite(endTime) ||
        endTime < source.time()) {
        prediction.status = StepStatus::InvalidTimeStep;
        return prediction;
    }

    Simulation branch = source;
    std::sort(maneuvers.begin(), maneuvers.end(), [](const ImpulseManeuver& a, const ImpulseManeuver& b) {
        return a.time < b.time;
    });
    maneuvers.erase(std::remove_if(maneuvers.begin(), maneuvers.end(), [&](const ImpulseManeuver& maneuver) {
        return maneuver.time < branch.time() - Epsilon || maneuver.time > endTime + Epsilon;
    }), maneuvers.end());

    prediction.trajectories.reserve(branch.bodies().size());
    for (const BodyState& body : branch.bodies()) {
        prediction.trajectories.push_back({body.id, {{branch.time(), body.position, body.velocity}}});
    }

    std::size_t maneuverIndex = 0;
    double nextSample = std::min(endTime, branch.time() + samplePeriod);
    while (branch.time() < endTime - Epsilon) {
        double target = nextSample;
        if (maneuverIndex < maneuvers.size()) {
            target = std::min(target, maneuvers[maneuverIndex].time);
        }
        target = std::min(target, endTime);
        if (target > branch.time() + Epsilon) {
            AdvanceReport report = branch.advance_to(target);
            prediction.events.insert(prediction.events.end(), report.events.begin(), report.events.end());
            if (report.status != StepStatus::Success) {
                prediction.status = report.status;
                prediction.endTime = branch.time();
                return prediction;
            }
        }

        while (maneuverIndex < maneuvers.size() && std::abs(maneuvers[maneuverIndex].time - branch.time()) <= Epsilon) {
            if (BodyState* body = branch.find_body(maneuvers[maneuverIndex].body)) {
                body->velocity += maneuvers[maneuverIndex].deltaVelocity;
            }
            ++maneuverIndex;
        }

        if (branch.time() >= nextSample - Epsilon || branch.time() >= endTime - Epsilon) {
            for (std::size_t i = 0; i < branch.bodies().size() && i < prediction.trajectories.size(); ++i) {
                const BodyState& body = branch.bodies()[i];
                prediction.trajectories[i].points.push_back({branch.time(), body.position, body.velocity});
            }
            nextSample = std::min(endTime, nextSample + samplePeriod);
        }
    }
    return prediction;
}

} // namespace phyz::engine
