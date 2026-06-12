#include "NBodySystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

namespace phyz {
namespace {

constexpr std::size_t MaxTrailPoints = 4200;
constexpr double TrailSamplePeriodYears = 0.004;
constexpr double AstronomicalG = 4.0 * Pi * Pi;
constexpr double SolarRadiusAu = 0.00465047;

Body makeBody(
    std::string name,
    double mass,
    double radius,
    double physicalRadius,
    Color color,
    Vec3 position,
    Vec3 velocity) {
    Body body;
    body.name = std::move(name);
    body.mass = mass;
    body.radius = radius;
    body.physicalRadius = physicalRadius > 0.0
        ? physicalRadius
        : SolarRadiusAu * std::pow(std::max(0.05, mass), 0.8);
    body.color = color;
    body.position = position;
    body.velocity = velocity;
    return body;
}

Vec3 scaled(Vec3 value, double scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Color paletteColor(std::size_t index) {
    constexpr Color palette[] = {
        {1.00f, 0.38f, 0.22f, 1.0f},
        {0.30f, 0.66f, 1.00f, 1.0f},
        {0.94f, 0.86f, 0.38f, 1.0f},
        {0.55f, 1.00f, 0.72f, 1.0f},
        {0.82f, 0.55f, 1.00f, 1.0f},
        {1.00f, 0.62f, 0.74f, 1.0f},
    };
    return palette[index % std::size(palette)];
}

Vec3 generatedPosition(std::size_t index, int bodyCount) {
    if (bodyCount == 1) {
        return {0.0, 0.0, 0.0};
    }

    const double i = static_cast<double>(index);
    const double count = static_cast<double>(std::max(2, bodyCount));
    const double angle = i * 2.39996322972865332;
    const double height = -0.85 + 1.70 * (i / (count - 1.0));
    const double radius = 1.25 + 0.22 * std::sin(i * 1.37);
    const double ring = radius * std::sqrt(std::max(0.12, 1.0 - height * height * 0.45));
    return {
        ring * std::cos(angle),
        ring * std::sin(angle),
        height,
    };
}

Vec3 generatedVelocity(const Vec3& position, std::size_t index, double totalMass, double gravitationalConstant) {
    if (lengthSquared(position) <= 1.0e-10) {
        return {0.0, 0.0, 0.0};
    }

    Vec3 tangent = cross({0.0, 0.0, 1.0}, position);
    if (lengthSquared(tangent) <= 1.0e-10) {
        tangent = cross({0.0, 1.0, 0.0}, position);
    }

    const double distance = std::max(0.35, length(position));
    const double speed = 0.20 * std::sqrt(gravitationalConstant * std::max(0.1, totalMass) / (distance + 0.45));
    Vec3 velocity = normalized(tangent) * speed;
    velocity.z += 0.28 * std::sin(static_cast<double>(index) * 1.913);
    return velocity;
}

double maxPositionRadius(const std::vector<Body>& bodies) {
    double result = 0.0;
    for (const Body& body : bodies) {
        result = std::max(result, length(body.position));
    }
    return result;
}

Color massWeightedColor(const Body& a, const Body& b) {
    const double total = std::max(1.0e-9, a.mass + b.mass);
    return {
        static_cast<float>((a.color.r * a.mass + b.color.r * b.mass) / total),
        static_cast<float>((a.color.g * a.mass + b.color.g * b.mass) / total),
        static_cast<float>((a.color.b * a.mass + b.color.b * b.mass) / total),
        1.0f,
    };
}

} // namespace

NBodySystem::NBodySystem() {
    reset(Scenario::TrisolarisChaos);
}

void NBodySystem::reset(Scenario scenario) {
    scenario_ = scenario;
    gravitationalConstant_ = AstronomicalG;
    bodies_.clear();
    shadowBodies_.clear();
    testParticles_.clear();
    events_.clear();
    mergerCount_ = 0;
    lastCloseEventTime_ = -1.0e9;
    elapsedTime_ = 0.0;
    trailTimer_ = 0.0;

    const double velocityScale = std::sqrt(gravitationalConstant_);

    switch (scenario_) {
    case Scenario::Custom:
        scenario_ = Scenario::TrisolarisChaos;
        reset(Scenario::TrisolarisChaos);
        return;

    case Scenario::TrisolarisChaos:
        softening_ = SolarRadiusAu * 1.7;
        recommendedTimeStep_ = 0.00012;
        recommendedCameraDistance_ = 6.6;
        bodies_.push_back(makeBody(
            "Trisolaris Alpha",
            1.8,
            0.135,
            0.0,
            {1.00f, 0.38f, 0.22f, 1.0f},
            {-1.45, -0.10, 0.35},
            scaled({0.12, 0.42, -0.12}, velocityScale)));
        bodies_.push_back(makeBody(
            "Trisolaris Beta",
            1.0,
            0.110,
            0.0,
            {0.30f, 0.66f, 1.00f, 1.0f},
            {1.05, -0.75, -0.32},
            scaled({-0.38, 0.05, 0.28}, velocityScale)));
        bodies_.push_back(makeBody(
            "Trisolaris Gamma",
            0.8,
            0.100,
            0.0,
            {0.94f, 0.86f, 0.38f, 1.0f},
            {0.35, 1.10, 0.05},
            scaled({0.20, -0.62, -0.18}, velocityScale)));
        break;

    case Scenario::FigureEight:
        softening_ = SolarRadiusAu * 0.35;
        recommendedTimeStep_ = 0.00062;
        recommendedCameraDistance_ = 5.4;
        bodies_.push_back(makeBody(
            "Aurelia",
            1.0,
            0.090,
            0.0,
            {1.00f, 0.42f, 0.22f, 1.0f},
            {-0.97000436, 0.24308753, 0.0},
            scaled({0.4662036850, 0.4323657300, 0.0}, velocityScale)));
        bodies_.push_back(makeBody(
            "Borealis",
            1.0,
            0.090,
            0.0,
            {0.20f, 0.60f, 1.00f, 1.0f},
            {0.97000436, -0.24308753, 0.0},
            scaled({0.4662036850, 0.4323657300, 0.0}, velocityScale)));
        bodies_.push_back(makeBody(
            "Cygnus",
            1.0,
            0.090,
            0.0,
            {0.40f, 1.00f, 0.70f, 1.0f},
            {0.0, 0.0, 0.0},
            scaled({-0.9324073700, -0.8647314600, 0.0}, velocityScale)));
        break;

    case Scenario::InclinedDance: {
        softening_ = SolarRadiusAu * 0.35;
        recommendedTimeStep_ = 0.00055;
        recommendedCameraDistance_ = 6.3;

        const double radius = 1.28;
        const double speed = std::sqrt(gravitationalConstant_ / (std::sqrt(3.0) * radius));
        const double tilt = 0.62;
        const double c = std::cos(tilt);
        const double s = std::sin(tilt);
        const auto rotate = [c, s](Vec3 value) {
            return Vec3{value.x, value.y * c - value.z * s, value.y * s + value.z * c};
        };

        bodies_.push_back(makeBody(
            "Helion",
            1.0,
            0.100,
            0.0,
            {1.00f, 0.74f, 0.32f, 1.0f},
            rotate({radius, 0.0, 0.0}),
            rotate({0.0, speed, 0.0})));
        bodies_.push_back(makeBody(
            "Iris",
            1.0,
            0.100,
            0.0,
            {0.34f, 0.75f, 1.00f, 1.0f},
            rotate({-0.5 * radius, std::sqrt(3.0) * 0.5 * radius, 0.0}),
            rotate({-std::sqrt(3.0) * 0.5 * speed, -0.5 * speed, 0.0})));
        bodies_.push_back(makeBody(
            "Vega",
            1.0,
            0.100,
            0.0,
            {0.76f, 0.45f, 1.00f, 1.0f},
            rotate({-0.5 * radius, -std::sqrt(3.0) * 0.5 * radius, 0.0}),
            rotate({std::sqrt(3.0) * 0.5 * speed, -0.5 * speed, 0.0})));
        break;
    }

    case Scenario::HierarchicalTriple:
        softening_ = SolarRadiusAu * 0.7;
        recommendedTimeStep_ = 0.00040;
        recommendedCameraDistance_ = 8.2;
        bodies_.push_back(makeBody(
            "Primary",
            1.50,
            0.120,
            0.0,
            {1.00f, 0.62f, 0.28f, 1.0f},
            {-0.3778, 0.0, 0.04},
            scaled({0.0, 0.7920, 0.0}, velocityScale)));
        bodies_.push_back(makeBody(
            "Companion",
            1.20,
            0.105,
            0.0,
            {0.25f, 0.72f, 1.00f, 1.0f},
            {0.4722, 0.0, -0.05},
            scaled({0.0, -0.9900, 0.0}, velocityScale)));
        bodies_.push_back(makeBody(
            "Wanderer",
            0.28,
            0.070,
            0.0,
            {0.72f, 1.00f, 0.58f, 1.0f},
            {2.75, 0.0, 0.65},
            scaled({0.0, 0.88, -0.16}, velocityScale)));
        break;
    }

    normalizeCenterOfMass();
    calculateAccelerations(accelerations_);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].acceleration = accelerations_[i];
    }
    rebaselineDiagnostics();
    seedTrails();
    seedShadowSystem();
    seedTestParticles(scenario_ == Scenario::FigureEight ? 160 : 320);
}

void NBodySystem::reset(const InitialConditionConfig& config) {
    scenario_ = Scenario::Custom;
    gravitationalConstant_ = AstronomicalG;
    bodies_.clear();
    shadowBodies_.clear();
    testParticles_.clear();
    events_.clear();
    mergerCount_ = 0;
    lastCloseEventTime_ = -1.0e9;
    elapsedTime_ = 0.0;
    trailTimer_ = 0.0;

    const int bodyCount = std::clamp(config.bodyCount, 1, 64);
    bodies_.reserve(static_cast<std::size_t>(bodyCount));

    for (int i = 0; i < bodyCount; ++i) {
        const BodyInitialConfig* bodyConfig = nullptr;
        if (i < static_cast<int>(config.bodies.size())) {
            bodyConfig = &config.bodies[static_cast<std::size_t>(i)];
        }

        const double mass = bodyConfig != nullptr && bodyConfig->mass
            ? *bodyConfig->mass
            : 0.75 + 0.18 * static_cast<double>(i % 5);
        const double radius = bodyConfig != nullptr && bodyConfig->radius
            ? *bodyConfig->radius
            : 0.085 + 0.010 * static_cast<double>(i % 4);
        const double physicalRadius = bodyConfig != nullptr && bodyConfig->physicalRadius
            ? *bodyConfig->physicalRadius
            : 0.0;
        const Color color = bodyConfig != nullptr && bodyConfig->color
            ? *bodyConfig->color
            : paletteColor(static_cast<std::size_t>(i));
        const Vec3 position = bodyConfig != nullptr && bodyConfig->position
            ? *bodyConfig->position
            : generatedPosition(static_cast<std::size_t>(i), bodyCount);

        bodies_.push_back(makeBody(
            bodyConfig != nullptr && bodyConfig->name ? *bodyConfig->name : "Body " + std::to_string(i + 1),
            mass,
            radius,
            physicalRadius,
            color,
            position,
            {}));
    }

    double totalMass = 0.0;
    for (const Body& body : bodies_) {
        totalMass += body.mass;
    }

    for (int i = 0; i < bodyCount; ++i) {
        const BodyInitialConfig* bodyConfig = nullptr;
        if (i < static_cast<int>(config.bodies.size())) {
            bodyConfig = &config.bodies[static_cast<std::size_t>(i)];
        }
        bodies_[static_cast<std::size_t>(i)].velocity = bodyConfig != nullptr && bodyConfig->velocity
            ? *bodyConfig->velocity
            : generatedVelocity(
                bodies_[static_cast<std::size_t>(i)].position,
                static_cast<std::size_t>(i),
                totalMass,
                gravitationalConstant_);
    }

    softening_ = config.softening.value_or(SolarRadiusAu * std::max(0.8, std::sqrt(static_cast<double>(bodyCount)) * 0.55));
    recommendedTimeStep_ = config.physicsDt.value_or(std::max(2.0e-5, 0.00017 / std::sqrt(static_cast<double>(bodyCount))));
    recommendedCameraDistance_ = config.cameraDistance.value_or(clamp(maxPositionRadius(bodies_) * 3.0 + 2.5, 4.5, 80.0));

    normalizeCenterOfMass();
    calculateAccelerations(accelerations_);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].acceleration = accelerations_[i];
    }
    rebaselineDiagnostics();
    seedTrails();
    seedShadowSystem();
    seedTestParticles(std::clamp(bodyCount * 80, 160, 720));
}

const char* NBodySystem::scenarioName() const {
    switch (scenario_) {
    case Scenario::TrisolarisChaos:
        return "Trisolaris chaotic system";
    case Scenario::FigureEight:
        return "Figure-eight choreography";
    case Scenario::InclinedDance:
        return "Tilted Lagrange triangle";
    case Scenario::HierarchicalTriple:
        return "Binary star with wanderer";
    case Scenario::Custom:
        return "Custom initial conditions";
    }
    return "Unknown";
}

void NBodySystem::step(double dt) {
    if (bodies_.empty()) {
        return;
    }

    double remaining = std::abs(dt);
    const double direction = dt >= 0.0 ? 1.0 : -1.0;
    int subSteps = 0;

    while (remaining > 1.0e-14 && subSteps < 1024) {
        const double subStep = std::min(remaining, adaptiveSubStep(remaining));
        integrateYoshida4(direction * subStep);
        integrateShadowYoshida4(direction * subStep);
        integrateTestParticles(direction * subStep);
        remaining -= subStep;
        elapsedTime_ += direction * subStep;
        detectCloseEncounters();
        if (collisionMergingEnabled_) {
            resolveCollisions();
        }
        captureTrail(subStep);
        ++subSteps;
    }

    if (remaining > 1.0e-14) {
        integrateYoshida4(direction * remaining);
        integrateShadowYoshida4(direction * remaining);
        integrateTestParticles(direction * remaining);
        elapsedTime_ += direction * remaining;
        detectCloseEncounters();
        if (collisionMergingEnabled_) {
            resolveCollisions();
        }
        captureTrail(remaining);
    }
}

void NBodySystem::setBodyPosition(std::size_t index, Vec3 position) {
    if (index >= bodies_.size()) {
        return;
    }
    bodies_[index].position = position;
    calculateAccelerations(accelerations_);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].acceleration = accelerations_[i];
    }
    rebaselineDiagnostics();
    seedTrails();
    seedShadowSystem();
}

void NBodySystem::setBodyVelocity(std::size_t index, Vec3 velocity) {
    if (index >= bodies_.size()) {
        return;
    }
    bodies_[index].velocity = velocity;
    calculateAccelerations(accelerations_);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].acceleration = accelerations_[i];
    }
    rebaselineDiagnostics();
    seedShadowSystem();
}

void NBodySystem::rebaselineDiagnostics() {
    initialEnergy_ = totalEnergy();
    initialAngularMomentumMagnitude_ = length(totalAngularMomentum());
}

void NBodySystem::seedTestParticles(int count) {
    testParticles_.clear();
    if (bodies_.empty() || count <= 0) {
        return;
    }

    const double systemRadius = std::max(1.4, maxPositionRadius(bodies_) + 0.8);
    testParticles_.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(count);
        const double angle = 2.0 * Pi * t * 5.0 + 0.37 * static_cast<double>(i % 11);
        const double band = static_cast<double>((i % 9) - 4) / 4.0;
        const double radius = systemRadius * (0.55 + 0.65 * static_cast<double>((i * 37) % count) / static_cast<double>(count));
        Vec3 position{
            radius * std::cos(angle),
            radius * std::sin(angle),
            band * 0.32 * systemRadius,
        };

        Vec3 tangent = normalized(cross({0.0, 0.0, 1.0}, position));
        if (lengthSquared(tangent) <= 1.0e-10) {
            tangent = {1.0, 0.0, 0.0};
        }
        const double speed = 0.42 * std::sqrt(gravitationalConstant_ * std::max(0.1, totalMass()) / std::max(0.4, length(position)));

        TestParticle particle;
        particle.position = position;
        particle.velocity = tangent * speed;
        particle.color = {0.62f, 0.86f, 1.00f, 0.35f};
        particle.trail.push_back(position);
        testParticles_.push_back(particle);
    }
}

double NBodySystem::totalEnergy() const {
    double kinetic = 0.0;
    double potential = 0.0;

    for (const Body& body : bodies_) {
        kinetic += 0.5 * body.mass * lengthSquared(body.velocity);
    }

    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
            const Vec3 delta = bodies_[j].position - bodies_[i].position;
            const double distance = std::sqrt(lengthSquared(delta) + softening_ * softening_);
            potential -= gravitationalConstant_ * bodies_[i].mass * bodies_[j].mass / distance;
        }
    }

    return kinetic + potential;
}

double NBodySystem::energyDrift() const {
    if (std::abs(initialEnergy_) <= 1.0e-12) {
        return 0.0;
    }
    return (totalEnergy() - initialEnergy_) / std::abs(initialEnergy_);
}

double NBodySystem::totalMass() const {
    double result = 0.0;
    for (const Body& body : bodies_) {
        result += body.mass;
    }
    return result;
}

Vec3 NBodySystem::centerOfMass() const {
    Vec3 result{};
    const double mass = totalMass();
    if (mass <= 0.0) {
        return result;
    }
    for (const Body& body : bodies_) {
        result += body.position * body.mass;
    }
    return result / mass;
}

Vec3 NBodySystem::totalAngularMomentum() const {
    Vec3 result{};
    for (const Body& body : bodies_) {
        result += cross(body.position, body.velocity) * body.mass;
    }
    return result;
}

double NBodySystem::angularMomentumDrift() const {
    if (initialAngularMomentumMagnitude_ <= 1.0e-12) {
        return length(totalAngularMomentum());
    }
    return (length(totalAngularMomentum()) - initialAngularMomentumMagnitude_) / initialAngularMomentumMagnitude_;
}

double NBodySystem::minSeparation() const {
    if (bodies_.size() < 2) {
        return 0.0;
    }

    double result = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
            result = std::min(result, length(bodies_[j].position - bodies_[i].position));
        }
    }
    return result;
}

double NBodySystem::chaosDivergence() const {
    if (bodies_.empty() || shadowBodies_.empty()) {
        return 0.0;
    }
    const std::size_t count = std::min(bodies_.size(), shadowBodies_.size());
    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        sum += length(bodies_[i].position - shadowBodies_[i].position);
    }
    return sum / static_cast<double>(count);
}

Vec3 NBodySystem::accelerationAt(Vec3 position) const {
    Vec3 result{};
    for (const Body& body : bodies_) {
        const Vec3 delta = body.position - position;
        const double softenedDistanceSquared = lengthSquared(delta) + softening_ * softening_;
        const double inverseDistance = 1.0 / std::sqrt(softenedDistanceSquared);
        result += delta * (gravitationalConstant_ * body.mass * inverseDistance * inverseDistance * inverseDistance);
    }
    return result;
}

const char* NBodySystem::systemStatus() const {
    if (bodies_.size() <= 1) {
        return "single remnant";
    }
    if (minSeparation() < 0.16) {
        return "close encounter";
    }

    const Vec3 center = centerOfMass();
    for (const Body& body : bodies_) {
        const Vec3 radial = body.position - center;
        if (length(radial) > 6.0 && dot(radial, body.velocity) > 0.0) {
            return "escape likely";
        }
    }

    return totalEnergy() < 0.0 ? "bound chaotic system" : "dispersing system";
}

void NBodySystem::calculateAccelerations(std::vector<Vec3>& output) const {
    calculateAccelerationsFor(bodies_, output);
}

void NBodySystem::calculateAccelerationsFor(const std::vector<Body>& source, std::vector<Vec3>& output) const {
    output.assign(source.size(), Vec3{});

    for (std::size_t i = 0; i < source.size(); ++i) {
        for (std::size_t j = i + 1; j < source.size(); ++j) {
            const Vec3 delta = source[j].position - source[i].position;
            const double softenedDistanceSquared = lengthSquared(delta) + softening_ * softening_;
            const double inverseDistance = 1.0 / std::sqrt(softenedDistanceSquared);
            const double inverseDistanceCubed = inverseDistance * inverseDistance * inverseDistance;

            const Vec3 direction = delta * inverseDistanceCubed;
            output[i] += direction * (gravitationalConstant_ * source[j].mass);
            output[j] -= direction * (gravitationalConstant_ * source[i].mass);
        }
    }
}

void NBodySystem::integrateYoshida4(double dt) {
    const double cubeRootTwo = std::cbrt(2.0);
    const double w1 = 1.0 / (2.0 - cubeRootTwo);
    const double w0 = -cubeRootTwo / (2.0 - cubeRootTwo);

    integrateLeapfrog(w1 * dt);
    integrateLeapfrog(w0 * dt);
    integrateLeapfrog(w1 * dt);
}

void NBodySystem::integrateLeapfrog(double dt) {
    calculateAccelerations(accelerations_);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].velocity += accelerations_[i] * (0.5 * dt);
        bodies_[i].position += bodies_[i].velocity * dt;
    }

    calculateAccelerations(accelerations_);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].velocity += accelerations_[i] * (0.5 * dt);
        bodies_[i].acceleration = accelerations_[i];
    }
}

void NBodySystem::integrateShadowYoshida4(double dt) {
    if (shadowBodies_.empty() || shadowBodies_.size() != bodies_.size()) {
        return;
    }

    const double cubeRootTwo = std::cbrt(2.0);
    const double w1 = 1.0 / (2.0 - cubeRootTwo);
    const double w0 = -cubeRootTwo / (2.0 - cubeRootTwo);

    integrateShadowLeapfrog(w1 * dt);
    integrateShadowLeapfrog(w0 * dt);
    integrateShadowLeapfrog(w1 * dt);
}

void NBodySystem::integrateShadowLeapfrog(double dt) {
    calculateAccelerationsFor(shadowBodies_, shadowAccelerations_);
    for (std::size_t i = 0; i < shadowBodies_.size(); ++i) {
        shadowBodies_[i].velocity += shadowAccelerations_[i] * (0.5 * dt);
        shadowBodies_[i].position += shadowBodies_[i].velocity * dt;
    }

    calculateAccelerationsFor(shadowBodies_, shadowAccelerations_);
    for (std::size_t i = 0; i < shadowBodies_.size(); ++i) {
        shadowBodies_[i].velocity += shadowAccelerations_[i] * (0.5 * dt);
        shadowBodies_[i].acceleration = shadowAccelerations_[i];
    }
}

void NBodySystem::integrateTestParticles(double dt) {
    for (TestParticle& particle : testParticles_) {
        const Vec3 a0 = accelerationAt(particle.position);
        particle.velocity += a0 * (0.5 * dt);
        particle.position += particle.velocity * dt;
        const Vec3 a1 = accelerationAt(particle.position);
        particle.velocity += a1 * (0.5 * dt);
    }
}

double NBodySystem::adaptiveSubStep(double requestedDt) const {
    const double maxDt = std::abs(requestedDt);
    if (maxDt <= 0.0 || bodies_.size() < 2) {
        return maxDt;
    }

    const double closest = std::max(minSeparation(), softening_ * 4.0);
    double maxSpeed = 0.0;
    double maxAcceleration = 0.0;

    std::vector<Vec3> accelerations;
    calculateAccelerations(accelerations);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        maxSpeed = std::max(maxSpeed, length(bodies_[i].velocity));
        maxAcceleration = std::max(maxAcceleration, length(accelerations[i]));
    }

    double candidate = maxDt;
    if (maxSpeed > 1.0e-12) {
        candidate = std::min(candidate, 0.035 * closest / maxSpeed);
    }
    if (maxAcceleration > 1.0e-12) {
        candidate = std::min(candidate, std::sqrt(0.010 * closest / maxAcceleration));
    }

    return clamp(candidate, 1.0e-7, maxDt);
}

void NBodySystem::normalizeCenterOfMass() {
    double totalMass = 0.0;
    Vec3 centerOfMass{};
    Vec3 centerVelocity{};

    for (const Body& body : bodies_) {
        totalMass += body.mass;
        centerOfMass += body.position * body.mass;
        centerVelocity += body.velocity * body.mass;
    }

    if (totalMass <= 0.0) {
        return;
    }

    centerOfMass = centerOfMass / totalMass;
    centerVelocity = centerVelocity / totalMass;

    for (Body& body : bodies_) {
        body.position -= centerOfMass;
        body.velocity -= centerVelocity;
    }
}

void NBodySystem::seedTrails() {
    for (Body& body : bodies_) {
        body.trail.clear();
        body.trail.push_back(body.position);
    }
}

void NBodySystem::seedShadowSystem() {
    shadowBodies_ = bodies_;
    if (!shadowBodies_.empty()) {
        shadowBodies_[0].position.x += 1.0e-6;
        shadowBodies_[0].trail.clear();
    }
    calculateAccelerationsFor(shadowBodies_, shadowAccelerations_);
}

void NBodySystem::captureTrail(double dt) {
    trailTimer_ += dt;
    if (trailTimer_ < TrailSamplePeriodYears) {
        return;
    }
    trailTimer_ = 0.0;

    for (Body& body : bodies_) {
        body.trail.push_back(body.position);
        while (body.trail.size() > MaxTrailPoints) {
            body.trail.pop_front();
        }
    }

    constexpr std::size_t MaxParticleTrailPoints = 360;
    for (TestParticle& particle : testParticles_) {
        particle.trail.push_back(particle.position);
        while (particle.trail.size() > MaxParticleTrailPoints) {
            particle.trail.pop_front();
        }
    }
}

void NBodySystem::detectCloseEncounters() {
    if (bodies_.size() < 2 || elapsedTime_ - lastCloseEventTime_ < 0.06) {
        return;
    }

    double closest = std::numeric_limits<double>::infinity();
    std::size_t first = 0;
    std::size_t second = 1;
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
            const double distance = length(bodies_[j].position - bodies_[i].position);
            if (distance < closest) {
                closest = distance;
                first = i;
                second = j;
            }
        }
    }

    const double threshold = std::max(0.12, softening_ * 14.0);
    if (closest < threshold) {
        std::ostringstream message;
        message << "close encounter: " << bodies_[first].name << " / " << bodies_[second].name
                << " at " << closest << " AU";
        pushEvent(message.str(), {0.90f, 0.78f, 0.42f, 1.0f});
        lastCloseEventTime_ = elapsedTime_;
    }
}

void NBodySystem::resolveCollisions() {
    bool merged = true;
    while (merged && bodies_.size() >= 2) {
        merged = false;
        for (std::size_t i = 0; i < bodies_.size() && !merged; ++i) {
            for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
                const double distance = length(bodies_[j].position - bodies_[i].position);
                const double collisionDistance = bodies_[i].physicalRadius + bodies_[j].physicalRadius;
                if (distance <= collisionDistance) {
                    mergeBodies(i, j);
                    merged = true;
                    break;
                }
            }
        }
    }
}

void NBodySystem::mergeBodies(std::size_t first, std::size_t second) {
    if (first >= bodies_.size() || second >= bodies_.size() || first == second) {
        return;
    }
    if (second < first) {
        std::swap(first, second);
    }

    Body& a = bodies_[first];
    const Body& b = bodies_[second];
    const double combinedMass = a.mass + b.mass;
    const Vec3 combinedPosition = (a.position * a.mass + b.position * b.mass) / combinedMass;
    const Vec3 combinedVelocity = (a.velocity * a.mass + b.velocity * b.mass) / combinedMass;
    const double combinedRadius = std::cbrt(a.radius * a.radius * a.radius + b.radius * b.radius * b.radius);
    const double combinedPhysicalRadius = std::cbrt(
        a.physicalRadius * a.physicalRadius * a.physicalRadius +
        b.physicalRadius * b.physicalRadius * b.physicalRadius);
    const std::string name = a.name + "+" + b.name;

    std::ostringstream message;
    message << "merger: " << a.name << " + " << b.name
            << " -> " << combinedMass << " solar masses";

    a.name = name;
    a.mass = combinedMass;
    a.radius = combinedRadius;
    a.physicalRadius = combinedPhysicalRadius;
    a.color = massWeightedColor(a, b);
    a.position = combinedPosition;
    a.velocity = combinedVelocity;
    a.trail.clear();
    a.trail.push_back(a.position);

    bodies_.erase(bodies_.begin() + static_cast<std::ptrdiff_t>(second));
    ++mergerCount_;

    calculateAccelerations(accelerations_);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].acceleration = accelerations_[i];
    }
    rebaselineDiagnostics();
    seedShadowSystem();
    pushEvent(message.str(), {1.00f, 0.44f, 0.32f, 1.0f});
}

void NBodySystem::pushEvent(std::string message, Color color) {
    events_.push_front({elapsedTime_, std::move(message), color});
    while (events_.size() > 8) {
        events_.pop_back();
    }
}

} // namespace phyz
