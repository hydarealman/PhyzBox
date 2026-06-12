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
constexpr double SpeedOfLightAuPerYear = 63241.07708426628;
constexpr double SolarRadiusAu = 0.00465047;

double schwarzschildRadius(double massSolar) {
    return 2.0 * AstronomicalG * massSolar / (SpeedOfLightAuPerYear * SpeedOfLightAuPerYear);
}

bool isCompactObject(BodyType type) {
    return type == BodyType::BlackHole || type == BodyType::NeutronStar || type == BodyType::WhiteDwarf;
}

double defaultPhysicalRadius(BodyType type, double mass) {
    const double safeMass = std::max(0.001, mass);
    switch (type) {
    case BodyType::BlackHole:
        return schwarzschildRadius(safeMass);
    case BodyType::NeutronStar:
        return 8.0e-8;
    case BodyType::WhiteDwarf:
        return 0.000045 * std::pow(safeMass, -1.0 / 3.0);
    case BodyType::Planet:
        return 0.00042 * std::pow(safeMass / 0.001, 1.0 / 3.0);
    case BodyType::MinorBody:
        return 0.000035 * std::pow(safeMass / 1.0e-6, 1.0 / 3.0);
    case BodyType::Star:
        return SolarRadiusAu * std::pow(safeMass, 0.8);
    }
    return SolarRadiusAu;
}

double densityFor(double mass, double physicalRadius) {
    const double radius = std::max(1.0e-10, physicalRadius);
    return mass / (radius * radius * radius);
}

double defaultTemperature(BodyType type, double mass) {
    const double safeMass = std::max(0.001, mass);
    switch (type) {
    case BodyType::BlackHole:
        return 0.0;
    case BodyType::NeutronStar:
        return 700000.0;
    case BodyType::WhiteDwarf:
        return 14000.0;
    case BodyType::Planet:
        return 280.0;
    case BodyType::MinorBody:
        return 180.0;
    case BodyType::Star:
        return 5778.0 * std::pow(safeMass, 0.52);
    }
    return 5778.0;
}

double defaultLuminosity(BodyType type, double mass) {
    const double safeMass = std::max(0.001, mass);
    switch (type) {
    case BodyType::BlackHole:
        return 0.0;
    case BodyType::NeutronStar:
        return 0.03;
    case BodyType::WhiteDwarf:
        return 0.02;
    case BodyType::Planet:
    case BodyType::MinorBody:
        return 0.0;
    case BodyType::Star:
        return std::pow(safeMass, 3.5);
    }
    return 0.0;
}

Color temperatureColor(double kelvin) {
    if (kelvin <= 0.0) {
        return {0.02f, 0.02f, 0.025f, 1.0f};
    }

    const double t = clamp(kelvin / 100.0, 10.0, 400.0);
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;

    if (t <= 66.0) {
        r = 255.0;
        g = 99.4708025861 * std::log(t) - 161.1195681661;
        b = t <= 19.0 ? 0.0 : 138.5177312231 * std::log(t - 10.0) - 305.0447927307;
    } else {
        r = 329.698727446 * std::pow(t - 60.0, -0.1332047592);
        g = 288.1221695283 * std::pow(t - 60.0, -0.0755148492);
        b = 255.0;
    }

    return {
        static_cast<float>(clamp(r / 255.0, 0.0, 1.0)),
        static_cast<float>(clamp(g / 255.0, 0.0, 1.0)),
        static_cast<float>(clamp(b / 255.0, 0.0, 1.0)),
        1.0f,
    };
}

Color defaultColorFor(BodyType type, double mass) {
    switch (type) {
    case BodyType::BlackHole:
        return {0.015f, 0.012f, 0.018f, 1.0f};
    case BodyType::NeutronStar:
        return {0.70f, 0.88f, 1.00f, 1.0f};
    case BodyType::WhiteDwarf:
        return {0.82f, 0.90f, 1.00f, 1.0f};
    case BodyType::Planet:
        return {0.35f, 0.72f, 0.48f, 1.0f};
    case BodyType::MinorBody:
        return {0.58f, 0.52f, 0.45f, 1.0f};
    case BodyType::Star:
        return temperatureColor(defaultTemperature(type, mass));
    }
    return {1.0f, 1.0f, 1.0f, 1.0f};
}

Body makeBody(
    std::string name,
    BodyType type,
    double mass,
    double radius,
    double physicalRadius,
    Color color,
    Vec3 position,
    Vec3 velocity) {
    Body body;
    body.name = std::move(name);
    body.type = type;
    body.mass = mass;
    body.radius = radius;
    body.physicalRadius = physicalRadius > 0.0
        ? physicalRadius
        : defaultPhysicalRadius(type, mass);
    body.density = densityFor(mass, body.physicalRadius);
    body.temperature = defaultTemperature(type, mass);
    body.luminosity = defaultLuminosity(type, mass);
    body.schwarzschildRadius = type == BodyType::BlackHole ? schwarzschildRadius(mass) : 0.0;
    body.innermostStableCircularOrbit = body.schwarzschildRadius * 3.0;
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

double rocheLimit(const Body& primary, const Body& secondary) {
    if (secondary.mass <= 0.0 || secondary.physicalRadius <= 0.0 ||
        secondary.type == BodyType::BlackHole || secondary.type == BodyType::NeutronStar) {
        return 0.0;
    }

    if (primary.type == BodyType::BlackHole) {
        return secondary.physicalRadius * std::cbrt(2.0 * primary.mass / secondary.mass);
    }

    const double densityRatio = std::max(1.0e-12, primary.density / std::max(1.0e-12, secondary.density));
    return 2.44 * primary.physicalRadius * std::cbrt(densityRatio);
}

Vec3 accelerationContribution(const Body& source, const Vec3& delta, double softening, double gravitationalConstant) {
    const double distanceSquared = lengthSquared(delta);
    if (distanceSquared <= 1.0e-18) {
        return {};
    }

    const double distance = std::sqrt(distanceSquared);
    if (source.type == BodyType::BlackHole && source.schwarzschildRadius > 0.0) {
        const double effective = std::max(distance - source.schwarzschildRadius, source.schwarzschildRadius * 0.25);
        const double scale = gravitationalConstant * source.mass / (distance * effective * effective);
        return delta * scale;
    }

    const double softenedDistanceSquared = distanceSquared + softening * softening;
    const double inverseDistance = 1.0 / std::sqrt(softenedDistanceSquared);
    const double inverseDistanceCubed = inverseDistance * inverseDistance * inverseDistance;
    return delta * (gravitationalConstant * source.mass * inverseDistanceCubed);
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
            BodyType::Star,
            1.8,
            0.135,
            0.0,
            {1.00f, 0.38f, 0.22f, 1.0f},
            {-1.45, -0.10, 0.35},
            scaled({0.12, 0.42, -0.12}, velocityScale)));
        bodies_.push_back(makeBody(
            "Trisolaris Beta",
            BodyType::Star,
            1.0,
            0.110,
            0.0,
            {0.30f, 0.66f, 1.00f, 1.0f},
            {1.05, -0.75, -0.32},
            scaled({-0.38, 0.05, 0.28}, velocityScale)));
        bodies_.push_back(makeBody(
            "Trisolaris Gamma",
            BodyType::Star,
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
            BodyType::Star,
            1.0,
            0.090,
            0.0,
            {1.00f, 0.42f, 0.22f, 1.0f},
            {-0.97000436, 0.24308753, 0.0},
            scaled({0.4662036850, 0.4323657300, 0.0}, velocityScale)));
        bodies_.push_back(makeBody(
            "Borealis",
            BodyType::Star,
            1.0,
            0.090,
            0.0,
            {0.20f, 0.60f, 1.00f, 1.0f},
            {0.97000436, -0.24308753, 0.0},
            scaled({0.4662036850, 0.4323657300, 0.0}, velocityScale)));
        bodies_.push_back(makeBody(
            "Cygnus",
            BodyType::Star,
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
            BodyType::Star,
            1.0,
            0.100,
            0.0,
            {1.00f, 0.74f, 0.32f, 1.0f},
            rotate({radius, 0.0, 0.0}),
            rotate({0.0, speed, 0.0})));
        bodies_.push_back(makeBody(
            "Iris",
            BodyType::Star,
            1.0,
            0.100,
            0.0,
            {0.34f, 0.75f, 1.00f, 1.0f},
            rotate({-0.5 * radius, std::sqrt(3.0) * 0.5 * radius, 0.0}),
            rotate({-std::sqrt(3.0) * 0.5 * speed, -0.5 * speed, 0.0})));
        bodies_.push_back(makeBody(
            "Vega",
            BodyType::Star,
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
            BodyType::Star,
            1.50,
            0.120,
            0.0,
            {1.00f, 0.62f, 0.28f, 1.0f},
            {-0.3778, 0.0, 0.04},
            scaled({0.0, 0.7920, 0.0}, velocityScale)));
        bodies_.push_back(makeBody(
            "Companion",
            BodyType::Star,
            1.20,
            0.105,
            0.0,
            {0.25f, 0.72f, 1.00f, 1.0f},
            {0.4722, 0.0, -0.05},
            scaled({0.0, -0.9900, 0.0}, velocityScale)));
        bodies_.push_back(makeBody(
            "Wanderer",
            BodyType::Star,
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
        const BodyType type = bodyConfig != nullptr && bodyConfig->type
            ? *bodyConfig->type
            : BodyType::Star;
        const double radius = bodyConfig != nullptr && bodyConfig->radius
            ? *bodyConfig->radius
            : (type == BodyType::BlackHole ? 0.145 : 0.085 + 0.010 * static_cast<double>(i % 4));
        const double physicalRadius = bodyConfig != nullptr && bodyConfig->physicalRadius
            ? *bodyConfig->physicalRadius
            : 0.0;
        const Color color = bodyConfig != nullptr && bodyConfig->color
            ? *bodyConfig->color
            : (bodyConfig != nullptr && bodyConfig->type ? defaultColorFor(type, mass) : paletteColor(static_cast<std::size_t>(i)));
        const Vec3 position = bodyConfig != nullptr && bodyConfig->position
            ? *bodyConfig->position
            : generatedPosition(static_cast<std::size_t>(i), bodyCount);

        bodies_.push_back(makeBody(
            bodyConfig != nullptr && bodyConfig->name ? *bodyConfig->name : "Body " + std::to_string(i + 1),
            type,
            mass,
            radius,
            physicalRadius,
            color,
            position,
            {}));

        Body& body = bodies_.back();
        if (bodyConfig != nullptr && bodyConfig->density) {
            body.density = *bodyConfig->density;
        }
        if (bodyConfig != nullptr && bodyConfig->temperature) {
            body.temperature = *bodyConfig->temperature;
            if (!bodyConfig->color) {
                body.color = temperatureColor(body.temperature);
            }
        }
        if (bodyConfig != nullptr && bodyConfig->luminosity) {
            body.luminosity = *bodyConfig->luminosity;
        }
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
        resolveBlackHoleCaptures();
        detectRocheEvents();
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
        resolveBlackHoleCaptures();
        detectRocheEvents();
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

double NBodySystem::maxTidalStress() const {
    double result = 0.0;
    for (const Body& body : bodies_) {
        result = std::max(result, body.tidalStress);
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
        result += accelerationContribution(body, body.position - position, softening_, gravitationalConstant_);
    }
    return result;
}

const char* NBodySystem::systemStatus() const {
    if (bodies_.size() <= 1) {
        return "single remnant";
    }
    if (maxTidalStress() > 0.75) {
        return "Roche limit risk";
    }
    for (const Body& primary : bodies_) {
        if (primary.type != BodyType::BlackHole) {
            continue;
        }
        for (const Body& body : bodies_) {
            if (&body == &primary) {
                continue;
            }
            const double distance = length(body.position - primary.position);
            if (distance < std::max(primary.innermostStableCircularOrbit * 12.0, primary.radius * 2.8)) {
                return "relativistic strong-field zone";
            }
        }
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
            output[i] += accelerationContribution(source[j], delta, softening_, gravitationalConstant_);
            output[j] += accelerationContribution(source[i], -delta, softening_, gravitationalConstant_);
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

void NBodySystem::resolveBlackHoleCaptures() {
    bool captured = true;
    while (captured && bodies_.size() >= 2) {
        captured = false;
        for (std::size_t i = 0; i < bodies_.size() && !captured; ++i) {
            if (bodies_[i].type != BodyType::BlackHole) {
                continue;
            }
            for (std::size_t j = 0; j < bodies_.size(); ++j) {
                if (i == j) {
                    continue;
                }
                const double distance = length(bodies_[j].position - bodies_[i].position);
                const double horizon = std::max(bodies_[i].schwarzschildRadius, bodies_[i].physicalRadius);
                if (distance <= horizon) {
                    absorbBody(i, j, "event horizon capture");
                    captured = true;
                    break;
                }
            }
        }
    }
}

void NBodySystem::detectRocheEvents() {
    for (Body& body : bodies_) {
        body.tidalStress = 0.0;
    }

    if (bodies_.size() < 2) {
        return;
    }

    for (std::size_t primaryIndex = 0; primaryIndex < bodies_.size(); ++primaryIndex) {
        for (std::size_t secondaryIndex = 0; secondaryIndex < bodies_.size(); ++secondaryIndex) {
            if (primaryIndex == secondaryIndex) {
                continue;
            }

            const Body& primary = bodies_[primaryIndex];
            const Body& secondary = bodies_[secondaryIndex];
            if (secondary.disrupted || isCompactObject(secondary.type)) {
                continue;
            }

            const double limit = rocheLimit(primary, secondary);
            if (limit <= 0.0) {
                continue;
            }

            const double distance = length(secondary.position - primary.position);
            if (distance <= 1.0e-12) {
                continue;
            }

            const double stress = limit / distance;
            bodies_[secondaryIndex].tidalStress = std::max(bodies_[secondaryIndex].tidalStress, stress);

            const bool strongMassRatio = primary.mass > secondary.mass * 3.0;
            const bool fragileBody = secondary.type == BodyType::Planet || secondary.type == BodyType::MinorBody;
            if (stress > 1.0 && (primary.type == BodyType::BlackHole || strongMassRatio || fragileBody)) {
                disruptBody(primaryIndex, secondaryIndex, limit);
                return;
            }
        }
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

    if (bodies_[first].type == BodyType::BlackHole) {
        absorbBody(first, second, "black hole collision capture");
        return;
    }
    if (bodies_[second].type == BodyType::BlackHole) {
        absorbBody(second, first, "black hole collision capture");
        return;
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
    const BodyType resultingType = b.mass > a.mass ? b.type : a.type;

    std::ostringstream message;
    message << "merger: " << a.name << " + " << b.name
            << " -> " << combinedMass << " solar masses";

    a.name = name;
    a.type = resultingType;
    a.mass = combinedMass;
    a.radius = combinedRadius;
    a.physicalRadius = combinedPhysicalRadius;
    a.density = densityFor(a.mass, a.physicalRadius);
    a.temperature = defaultTemperature(a.type, a.mass);
    a.luminosity = defaultLuminosity(a.type, a.mass);
    a.schwarzschildRadius = a.type == BodyType::BlackHole ? schwarzschildRadius(a.mass) : 0.0;
    a.innermostStableCircularOrbit = a.schwarzschildRadius * 3.0;
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

void NBodySystem::absorbBody(std::size_t absorber, std::size_t absorbed, const std::string& reason) {
    if (absorber >= bodies_.size() || absorbed >= bodies_.size() || absorber == absorbed) {
        return;
    }

    Body& primary = bodies_[absorber];
    const Body secondary = bodies_[absorbed];
    const double combinedMass = primary.mass + secondary.mass;
    primary.velocity = (primary.velocity * primary.mass + secondary.velocity * secondary.mass) / combinedMass;
    primary.position = (primary.position * primary.mass + secondary.position * secondary.mass) / combinedMass;
    primary.mass = combinedMass;
    primary.type = BodyType::BlackHole;
    primary.physicalRadius = schwarzschildRadius(primary.mass);
    primary.schwarzschildRadius = primary.physicalRadius;
    primary.innermostStableCircularOrbit = primary.schwarzschildRadius * 3.0;
    primary.density = densityFor(primary.mass, primary.physicalRadius);
    primary.temperature = 0.0;
    primary.luminosity = 0.0;
    primary.accretionDiskMass += secondary.mass * 0.18;
    primary.color = defaultColorFor(BodyType::BlackHole, primary.mass);
    primary.trail.clear();
    primary.trail.push_back(primary.position);

    std::ostringstream message;
    message << reason << ": " << primary.name << " swallowed " << secondary.name;

    bodies_.erase(bodies_.begin() + static_cast<std::ptrdiff_t>(absorbed));
    ++mergerCount_;

    pushEvent(message.str(), {0.75f, 0.62f, 1.00f, 1.0f});

    calculateAccelerations(accelerations_);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].acceleration = accelerations_[i];
    }
    rebaselineDiagnostics();
    seedShadowSystem();
}

void NBodySystem::disruptBody(std::size_t primary, std::size_t secondary, double limit) {
    if (primary >= bodies_.size() || secondary >= bodies_.size() || primary == secondary) {
        return;
    }

    Body& primaryBody = bodies_[primary];
    const Body source = bodies_[secondary];
    spawnDebris(source, primaryBody, source.type == BodyType::Star ? 120 : 70);
    if (primaryBody.type == BodyType::BlackHole) {
        primaryBody.accretionDiskMass += source.mass * 0.35;
    }

    std::ostringstream message;
    message << "tidal disruption: " << source.name << " crossed Roche limit near "
            << primaryBody.name << " (" << limit << " AU)";
    pushEvent(message.str(), {1.00f, 0.72f, 0.34f, 1.0f});

    bodies_.erase(bodies_.begin() + static_cast<std::ptrdiff_t>(secondary));
    calculateAccelerations(accelerations_);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].acceleration = accelerations_[i];
    }
    rebaselineDiagnostics();
    seedShadowSystem();
}

void NBodySystem::spawnDebris(const Body& source, const Body& primary, int count) {
    const Vec3 radial = normalized(source.position - primary.position);
    Vec3 tangent = normalized(cross({0.0, 0.0, 1.0}, radial));
    if (lengthSquared(tangent) <= 1.0e-10) {
        tangent = normalized(cross({0.0, 1.0, 0.0}, radial));
    }
    const Vec3 binormal = normalized(cross(radial, tangent));
    const double spread = std::max(source.physicalRadius * 5.0, 0.008);

    for (int i = 0; i < count; ++i) {
        const double phase = 2.0 * Pi * static_cast<double>(i) / static_cast<double>(std::max(1, count));
        const double offset = (static_cast<double>((i * 17) % count) / static_cast<double>(std::max(1, count)) - 0.5) * spread;
        TestParticle particle;
        particle.position = source.position +
            tangent * (std::cos(phase) * spread * 0.45) +
            binormal * (std::sin(phase) * spread * 0.25) +
            radial * offset;
        particle.velocity = source.velocity +
            tangent * (0.10 * std::sin(phase)) -
            radial * (0.04 * std::cos(phase));
        particle.color = source.type == BodyType::Planet
            ? Color{0.52f, 0.95f, 0.78f, 0.45f}
            : Color{1.00f, 0.72f, 0.38f, 0.50f};
        particle.trail.push_back(particle.position);
        testParticles_.push_back(particle);
    }
}

void NBodySystem::pushEvent(std::string message, Color color) {
    events_.push_front({elapsedTime_, std::move(message), color});
    while (events_.size() > 8) {
        events_.pop_back();
    }
}

} // namespace phyz
