#include "NBodySystem.hpp"
#include "phyz/libphyz.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <random>
#include <sstream>
#include <string>

namespace phyz {
namespace {

constexpr std::size_t MaxTrailPoints = 4200;
constexpr double TrailSamplePeriodYears = 0.004;
constexpr double AstronomicalG = 4.0 * Pi * Pi;
constexpr double SpeedOfLightAuPerYear = 63241.07708426628;
constexpr double SolarRadiusAu = 0.00465047;
constexpr double GoldenAngle = 2.39996322972865332;

double schwarzschildRadius(double massSolar) {
    return 2.0 * AstronomicalG * massSolar / (SpeedOfLightAuPerYear * SpeedOfLightAuPerYear);
}

bool isCompactObject(BodyType type) {
    return type == BodyType::BlackHole || type == BodyType::NeutronStar || type == BodyType::WhiteDwarf;
}

int firstSpacecraftIndex(const std::vector<Body>& bodies) {
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].type == BodyType::Spacecraft) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

double defaultPhysicalRadius(BodyType type, double mass) {
    switch (type) {
    case BodyType::BlackHole:
        return schwarzschildRadius(std::max(1.0e-12, mass));
    case BodyType::NeutronStar:
        return 8.0e-8;
    case BodyType::WhiteDwarf:
        return 0.000045 * std::pow(std::max(0.05, mass), -1.0 / 3.0);
    case BodyType::Planet:
        return 0.00042 * std::pow(std::max(1.0e-12, mass) / 0.001, 1.0 / 3.0);
    case BodyType::MinorBody:
        return 0.000035 * std::pow(std::max(1.0e-18, mass) / 1.0e-6, 1.0 / 3.0);
    case BodyType::Spacecraft:
        return 8.0e-8;
    case BodyType::Star:
        return SolarRadiusAu * std::pow(std::max(0.001, mass), 0.8);
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
    case BodyType::Spacecraft:
        return 290.0;
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
    case BodyType::Spacecraft:
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
    case BodyType::Spacecraft:
        return {0.95f, 0.98f, 1.00f, 1.0f};
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

double defaultRotationPeriod(BodyType type, double mass, std::size_t index) {
    const double indexOffset = 1.0 + 0.11 * static_cast<double>(index % 5);
    switch (type) {
    case BodyType::Star:
        return (0.045 + 0.018 / std::sqrt(std::max(0.2, mass))) * indexOffset;
    case BodyType::Planet:
        return 0.0027 * indexOffset;
    case BodyType::MinorBody:
        return 0.00075 * indexOffset;
    case BodyType::WhiteDwarf:
        return 0.006 * indexOffset;
    case BodyType::NeutronStar:
        return 0.00018 * indexOffset;
    case BodyType::BlackHole:
        return 0.018 * indexOffset;
    case BodyType::Spacecraft:
        return 0.0009 * indexOffset;
    }
    return 0.08;
}

Vec3 defaultSpinAxis(std::size_t index) {
    const double i = static_cast<double>(index);
    return normalized({
        0.36 * std::sin(i * 1.71 + 0.45),
        0.28 * std::cos(i * 0.91 + 1.10),
        1.0,
    });
}

void configureDefaultSpin(Body& body, std::size_t index) {
    body.spinAxis = defaultSpinAxis(index);
    body.rotationPeriod = defaultRotationPeriod(body.type, body.mass, index);
    body.rotationAngle = std::fmod(0.73 * static_cast<double>(index), 2.0 * Pi);
}

Vec3 scaled(Vec3 value, double scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 rotateOrbitPlane(Vec3 value, double inclination, double ascendingNode) {
    const double ci = std::cos(inclination);
    const double si = std::sin(inclination);
    const Vec3 tilted{value.x, value.y * ci - value.z * si, value.y * si + value.z * ci};
    const double cn = std::cos(ascendingNode);
    const double sn = std::sin(ascendingNode);
    return {
        tilted.x * cn - tilted.y * sn,
        tilted.x * sn + tilted.y * cn,
        tilted.z,
    };
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
        secondary.type == BodyType::BlackHole || secondary.type == BodyType::NeutronStar ||
        secondary.type == BodyType::Spacecraft) {
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

engine::Vec3d toEngine(Vec3 value) {
    return {value.x, value.y, value.z};
}

Vec3 fromEngine(engine::Vec3d value) {
    return {value.x, value.y, value.z};
}

std::vector<engine::BodyState> makeEngineStates(const std::vector<Body>& bodies) {
    std::vector<engine::BodyState> states;
    states.reserve(bodies.size());
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        states.push_back({
            engine::BodyId{static_cast<std::uint64_t>(i + 1)},
            body.mass,
            body.mass,
            body.physicalRadius,
            body.schwarzschildRadius,
            toEngine(body.position),
            toEngine(body.velocity),
            toEngine(body.acceleration),
        });
    }
    return states;
}

engine::ForcePipeline makeEngineForces(double gravitationalConstant, double softening) {
    engine::ForcePipeline forces;
    forces.add<engine::PairwiseGravity>(
        gravitationalConstant,
        engine::GravityLaw::PaczynskiWiita,
        softening);
    return forces;
}

void copyEngineState(const std::vector<engine::BodyState>& states, std::vector<Body>& bodies) {
    const std::size_t count = std::min(states.size(), bodies.size());
    for (std::size_t i = 0; i < count; ++i) {
        bodies[i].position = fromEngine(states[i].position);
        bodies[i].velocity = fromEngine(states[i].velocity);
        bodies[i].acceleration = fromEngine(states[i].acceleration);
    }
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
    lastAssistEventTime_ = -1.0e9;
    lastExplorationEventTime_ = -1.0e9;
    initialSpacecraftSpeed_ = 0.0;
    spacecraftNearestEncounterDistance_ = std::numeric_limits<double>::infinity();
    explorerControl_ = {};
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

    case Scenario::GravityAssist:
        softening_ = SolarRadiusAu * 0.35;
        recommendedTimeStep_ = 0.000045;
        recommendedCameraDistance_ = 6.2;
        bodies_.push_back(makeBody(
            "Helios",
            BodyType::Star,
            1.0,
            0.120,
            0.0,
            {1.00f, 0.68f, 0.32f, 1.0f},
            {0.0, 0.0, 0.0},
            {0.0, 0.0, 0.0}));
        bodies_.push_back(makeBody(
            "Asterion",
            BodyType::Planet,
            0.0015,
            0.070,
            0.00050,
            {0.34f, 0.70f, 1.00f, 1.0f},
            {1.60, 0.0, 0.03},
            {0.0, 4.98, 0.08}));
        bodies_.push_back(makeBody(
            "Daedalus Probe",
            BodyType::Spacecraft,
            1.0e-12,
            0.035,
            8.0e-8,
            {0.96f, 0.98f, 1.00f, 1.0f},
            {1.05, -0.65, 0.14},
            {3.65, 9.75, -0.45}));
        break;

    case Scenario::ProceduralUniverse:
        generateProceduralUniverse();
        normalizeCenterOfMass();
        calculateAccelerations(accelerations_);
        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            bodies_[i].acceleration = accelerations_[i];
        }
        rebaselineDiagnostics();
        seedTrails();
        shadowBodies_.clear();
        testParticles_.clear();
        pushEvent("self-consistent N-body planetary system initialized", {0.62f, 0.86f, 1.00f, 1.0f});
        return;
    }

    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        configureDefaultSpin(bodies_[i], i);
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
    lastAssistEventTime_ = -1.0e9;
    lastExplorationEventTime_ = -1.0e9;
    initialSpacecraftSpeed_ = 0.0;
    spacecraftNearestEncounterDistance_ = std::numeric_limits<double>::infinity();
    explorerControl_ = {};
    elapsedTime_ = 0.0;
    trailTimer_ = 0.0;

    const int bodyCount = std::clamp(config.bodyCount, 1, 64);
    bodies_.reserve(static_cast<std::size_t>(bodyCount));

    for (int i = 0; i < bodyCount; ++i) {
        const BodyInitialConfig* bodyConfig = nullptr;
        if (i < static_cast<int>(config.bodies.size())) {
            bodyConfig = &config.bodies[static_cast<std::size_t>(i)];
        }

        const BodyType type = bodyConfig != nullptr && bodyConfig->type
            ? *bodyConfig->type
            : BodyType::Star;
        const double defaultMass = type == BodyType::Spacecraft
            ? 1.0e-12
            : (type == BodyType::Planet ? 0.003 : 0.75 + 0.18 * static_cast<double>(i % 5));
        const double mass = bodyConfig != nullptr && bodyConfig->mass
            ? *bodyConfig->mass
            : defaultMass;
        const double radius = bodyConfig != nullptr && bodyConfig->radius
            ? *bodyConfig->radius
            : (type == BodyType::BlackHole
                ? 0.145
                : (type == BodyType::Spacecraft ? 0.035 : 0.085 + 0.010 * static_cast<double>(i % 4)));
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
        configureDefaultSpin(body, static_cast<std::size_t>(i));
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
        if (bodyConfig != nullptr && bodyConfig->spinAxis && lengthSquared(*bodyConfig->spinAxis) > 1.0e-12) {
            body.spinAxis = normalized(*bodyConfig->spinAxis);
        }
        if (bodyConfig != nullptr && bodyConfig->rotationPeriod) {
            body.rotationPeriod = *bodyConfig->rotationPeriod;
        }
        if (bodyConfig != nullptr && bodyConfig->rotationAngle) {
            body.rotationAngle = *bodyConfig->rotationAngle;
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
    case Scenario::GravityAssist:
        return "Gravity assist flyby";
    case Scenario::ProceduralUniverse:
        return "Self-consistent planetary system";
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
        if (scenario_ == Scenario::ProceduralUniverse) {
            applyExplorerPropulsion(direction * subStep * 0.5);
        }
        integrateYoshida4(direction * subStep);
        if (scenario_ == Scenario::ProceduralUniverse) {
            applyExplorerPropulsion(direction * subStep * 0.5);
        } else {
            integrateShadowYoshida4(direction * subStep);
            integrateTestParticles(direction * subStep);
        }
        advanceRotations(direction * subStep);
        remaining -= subStep;
        elapsedTime_ += direction * subStep;
        detectCloseEncounters();
        detectGravityAssist();
        detectExplorationEvents();
        resolveBlackHoleCaptures();
        detectRocheEvents();
        if (collisionMergingEnabled_) {
            resolveCollisions();
        }
        captureTrail(subStep);
        ++subSteps;
    }

    if (remaining > 1.0e-14) {
        if (scenario_ == Scenario::ProceduralUniverse) {
            applyExplorerPropulsion(direction * remaining * 0.5);
        }
        integrateYoshida4(direction * remaining);
        if (scenario_ == Scenario::ProceduralUniverse) {
            applyExplorerPropulsion(direction * remaining * 0.5);
        } else {
            integrateShadowYoshida4(direction * remaining);
            integrateTestParticles(direction * remaining);
        }
        advanceRotations(direction * remaining);
        elapsedTime_ += direction * remaining;
        detectCloseEncounters();
        detectGravityAssist();
        detectExplorationEvents();
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

void NBodySystem::setExplorerControlState(ExplorerControlState control) {
    explorerControl_ = control;
}

void NBodySystem::rebaselineDiagnostics() {
    initialEnergy_ = totalEnergy();
    initialAngularMomentumMagnitude_ = length(totalAngularMomentum());
    initialSpacecraftSpeed_ = spacecraftSpeed();
    spacecraftNearestEncounterDistance_ = std::numeric_limits<double>::infinity();
    spacecraftNearestEncounterDistance_ = spacecraftNearestEncounterDistance();
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
    const std::vector<engine::BodyState> states = makeEngineStates(bodies_);
    const engine::ForcePipeline forces = makeEngineForces(gravitationalConstant_, softening_);
    const engine::InvariantReport report = engine::calculate_invariants(states, forces, elapsedTime_);
    return report.mechanicalEnergy.value_or(report.kineticEnergy);
}

double NBodySystem::energyDrift() const {
    if (std::abs(initialEnergy_) <= 1.0e-12) {
        return 0.0;
    }
    return (totalEnergy() - initialEnergy_) / std::abs(initialEnergy_);
}

Vec3 NBodySystem::totalLinearMomentum() const {
    const std::vector<engine::BodyState> states = makeEngineStates(bodies_);
    const engine::ForcePipeline forces = makeEngineForces(gravitationalConstant_, softening_);
    return fromEngine(engine::calculate_invariants(states, forces, elapsedTime_).linearMomentum);
}

double NBodySystem::linearMomentumError() const {
    const std::vector<engine::BodyState> states = makeEngineStates(bodies_);
    const engine::ForcePipeline forces = makeEngineForces(gravitationalConstant_, softening_);
    return engine::calculate_invariants(states, forces, elapsedTime_).momentumResidual;
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
    const std::vector<engine::BodyState> states = makeEngineStates(bodies_);
    const engine::ForcePipeline forces = makeEngineForces(gravitationalConstant_, softening_);
    return fromEngine(engine::calculate_invariants(states, forces, elapsedTime_).angularMomentum);
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

double NBodySystem::spacecraftSpeed() const {
    const int index = firstSpacecraftIndex(bodies_);
    if (index < 0) {
        return 0.0;
    }
    return length(bodies_[static_cast<std::size_t>(index)].velocity);
}

double NBodySystem::spacecraftSpeedGain() const {
    if (initialSpacecraftSpeed_ <= 1.0e-12) {
        return 0.0;
    }
    return (spacecraftSpeed() - initialSpacecraftSpeed_) / initialSpacecraftSpeed_;
}

double NBodySystem::spacecraftNearestEncounterDistance() const {
    double currentNearest = std::numeric_limits<double>::infinity();
    const int craftIndex = firstSpacecraftIndex(bodies_);
    if (craftIndex < 0) {
        return 0.0;
    }

    const Body& craft = bodies_[static_cast<std::size_t>(craftIndex)];
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (static_cast<int>(i) == craftIndex || bodies_[i].type == BodyType::Spacecraft) {
            continue;
        }
        currentNearest = std::min(currentNearest, length(bodies_[i].position - craft.position));
    }

    const double stored = std::isfinite(spacecraftNearestEncounterDistance_)
        ? spacecraftNearestEncounterDistance_
        : currentNearest;
    const double best = std::min(stored, currentNearest);
    return std::isfinite(best) ? best : 0.0;
}

int NBodySystem::spacecraftIndex() const {
    return firstSpacecraftIndex(bodies_);
}

int NBodySystem::explorerNearestPlanetIndex() const {
    const int shipIndex = firstSpacecraftIndex(bodies_);
    if (shipIndex < 0) {
        return -1;
    }

    const Body& ship = bodies_[static_cast<std::size_t>(shipIndex)];
    int nearest = -1;
    double nearestDistance = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (static_cast<int>(i) == shipIndex) {
            continue;
        }
        if (bodies_[i].type != BodyType::Planet && bodies_[i].type != BodyType::MinorBody) {
            continue;
        }
        const double distance = length(bodies_[i].position - ship.position);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = static_cast<int>(i);
        }
    }
    return nearest;
}

double NBodySystem::explorerNearestPlanetDistance() const {
    const int shipIndex = firstSpacecraftIndex(bodies_);
    const int planetIndex = explorerNearestPlanetIndex();
    if (shipIndex < 0 || planetIndex < 0) {
        return 0.0;
    }
    return length(bodies_[static_cast<std::size_t>(planetIndex)].position -
        bodies_[static_cast<std::size_t>(shipIndex)].position);
}

double NBodySystem::explorerLocalGravity() const {
    const int shipIndex = firstSpacecraftIndex(bodies_);
    if (shipIndex < 0) {
        return 0.0;
    }
    return length(accelerationAt(bodies_[static_cast<std::size_t>(shipIndex)].position));
}

double NBodySystem::explorerEscapeSpeed() const {
    const int shipIndex = firstSpacecraftIndex(bodies_);
    const int planetIndex = explorerNearestPlanetIndex();
    if (shipIndex < 0 || planetIndex < 0) {
        return 0.0;
    }
    const Body& ship = bodies_[static_cast<std::size_t>(shipIndex)];
    const Body& planet = bodies_[static_cast<std::size_t>(planetIndex)];
    const double distance = std::max(planet.physicalRadius, length(planet.position - ship.position));
    return std::sqrt(2.0 * gravitationalConstant_ * planet.mass / distance);
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
    if (scenario_ == Scenario::ProceduralUniverse) {
        const int nearest = explorerNearestPlanetIndex();
        if (nearest >= 0) {
            const Body& body = bodies_[static_cast<std::size_t>(nearest)];
            const double distance = explorerNearestPlanetDistance();
            const int ship = firstSpacecraftIndex(bodies_);
            const double contactDistance = body.physicalRadius +
                (ship >= 0 ? bodies_[static_cast<std::size_t>(ship)].physicalRadius : 0.0);
            if (distance < contactDistance * 4.0) {
                return "physical collision proximity";
            }
            if (distance < planetaryInfluenceRadius(static_cast<std::size_t>(nearest))) {
                return "inside planetary Hill sphere";
            }
        }
        return "multi-body free flight";
    }
    if (bodies_.size() <= 1) {
        return "single remnant";
    }
    if (maxTidalStress() > 0.75) {
        return "Roche limit risk";
    }
    if (firstSpacecraftIndex(bodies_) >= 0 && spacecraftNearestEncounterDistance() < 0.22) {
        return "spacecraft gravity assist";
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
    const std::vector<engine::BodyState> states = makeEngineStates(source);
    const engine::ForcePipeline forces = makeEngineForces(gravitationalConstant_, softening_);
    std::vector<engine::Vec3d> engineOutput(source.size());
    forces.calculate(states, elapsedTime_, engineOutput);
    output.resize(engineOutput.size());
    std::transform(engineOutput.begin(), engineOutput.end(), output.begin(), fromEngine);
}

void NBodySystem::integrateYoshida4(double dt) {
    std::vector<engine::BodyState> states = makeEngineStates(bodies_);
    const engine::ForcePipeline forces = makeEngineForces(gravitationalConstant_, softening_);
    engine::Yoshida4Fixed integrator;
    const engine::StepResult result = integrator.step(states, forces, elapsedTime_, dt);
    if (result.status == engine::StepStatus::Success) {
        copyEngineState(states, bodies_);
    }
}

void NBodySystem::integrateLeapfrog(double dt) {
    std::vector<engine::BodyState> states = makeEngineStates(bodies_);
    const engine::ForcePipeline forces = makeEngineForces(gravitationalConstant_, softening_);
    engine::Leapfrog2Fixed integrator;
    const engine::StepResult result = integrator.step(states, forces, elapsedTime_, dt);
    if (result.status == engine::StepStatus::Success) {
        copyEngineState(states, bodies_);
    }
}

void NBodySystem::integrateShadowYoshida4(double dt) {
    if (shadowBodies_.empty() || shadowBodies_.size() != bodies_.size()) {
        return;
    }

    std::vector<engine::BodyState> states = makeEngineStates(shadowBodies_);
    const engine::ForcePipeline forces = makeEngineForces(gravitationalConstant_, softening_);
    engine::Yoshida4Fixed integrator;
    const engine::StepResult result = integrator.step(states, forces, elapsedTime_, dt);
    if (result.status == engine::StepStatus::Success) {
        copyEngineState(states, shadowBodies_);
    }
}

void NBodySystem::integrateShadowLeapfrog(double dt) {
    std::vector<engine::BodyState> states = makeEngineStates(shadowBodies_);
    const engine::ForcePipeline forces = makeEngineForces(gravitationalConstant_, softening_);
    engine::Leapfrog2Fixed integrator;
    const engine::StepResult result = integrator.step(states, forces, elapsedTime_, dt);
    if (result.status == engine::StepStatus::Success) {
        copyEngineState(states, shadowBodies_);
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

void NBodySystem::applyExplorerPropulsion(double dt) {
    const int shipIndex = firstSpacecraftIndex(bodies_);
    if (shipIndex < 0 || dt == 0.0) {
        return;
    }

    Body& ship = bodies_[static_cast<std::size_t>(shipIndex)];
    Vec3 commandedDirection = explorerControl_.thrustDirection;
    if (lengthSquared(commandedDirection) > 1.0) {
        commandedDirection = normalized(commandedDirection);
    }

    // Control input is modeled as finite spacecraft acceleration. "Brake" is a
    // retrograde burn; there is no drag, damping, speed cap, or surface bounce.
    const double mainEngineAcceleration = explorerControl_.boost ? 96.0 : 34.0;
    Vec3 propulsion = commandedDirection * mainEngineAcceleration;
    if (explorerControl_.brake && lengthSquared(ship.velocity) > 1.0e-18) {
        propulsion -= normalized(ship.velocity) * 34.0;
    }
    ship.velocity += propulsion * dt;
    ship.acceleration += propulsion;
}

void NBodySystem::generateProceduralUniverse() {
    bodies_.clear();
    shadowBodies_.clear();
    testParticles_.clear();
    events_.clear();
    gravitationalConstant_ = AstronomicalG;
    softening_ = 2.0e-5;
    recommendedTimeStep_ = 0.00020;
    recommendedCameraDistance_ = 3.4;

    Body star = makeBody(
        "Atlas Prime",
        BodyType::Star,
        1.35,
        0.22,
        0.0,
        {1.00f, 0.72f, 0.36f, 1.0f},
        {0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0});
    configureDefaultSpin(star, 0);
    bodies_.push_back(star);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    // A compact but genuinely live system: every body below participates in
    // the same pairwise force calculation. The logarithmic spacing and modest
    // masses avoid manufacturing a visually dense but dynamically impossible
    // 220-major-planet system.
    constexpr int PlanetCount = 32;
    bodies_.reserve(PlanetCount + 2);

    for (int i = 0; i < PlanetCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(PlanetCount - 1);
        const double semiMajorAxis = 0.72 * std::pow(34.0 / 0.72, t);
        const double eccentricity = 0.005 + unit(rng) * 0.045;
        const double orbitRadius = semiMajorAxis * (1.0 - eccentricity);
        const double mass = 3.0e-7 + std::pow(unit(rng), 2.2) * 2.8e-4;
        const double visualRadius = 0.055 + std::cbrt(mass / 0.001) * 0.052 + unit(rng) * 0.040;
        const bool icy = i % 7 == 0 || orbitRadius > 12.0;
        const bool hot = orbitRadius < 1.2 && i % 4 != 0;
        const bool ocean = !hot && !icy && i % 5 == 0;
        Color color = hot
            ? Color{0.96f, 0.44f, 0.22f, 1.0f}
            : (icy
                ? Color{0.56f, 0.82f, 1.00f, 1.0f}
                : (ocean
                    ? Color{0.22f, 0.56f, 0.86f, 1.0f}
                    : Color{0.34f + static_cast<float>(unit(rng) * 0.32),
                            0.52f + static_cast<float>(unit(rng) * 0.34),
                            0.30f + static_cast<float>(unit(rng) * 0.28),
                            1.0f}));

        Body planet = makeBody(
            "World " + std::to_string(i + 1),
            i % 13 == 0 ? BodyType::MinorBody : BodyType::Planet,
            mass,
            visualRadius,
            0.0,
            color,
            {},
            {});
        const double phase = GoldenAngle * static_cast<double>(i) + unit(rng) * 0.25;
        const double inclination = (unit(rng) - 0.5) * 0.12;
        const double ascendingNode = unit(rng) * 2.0 * Pi;
        planet.temperature = hot ? 740.0 : (icy ? 115.0 : (ocean ? 294.0 : 250.0 + unit(rng) * 170.0));
        planet.luminosity = 0.0;
        configureDefaultSpin(planet, static_cast<std::size_t>(i + 1));
        planet.rotationPeriod = 0.0014 + unit(rng) * 0.0085;
        planet.position = rotateOrbitPlane(
            {orbitRadius * std::cos(phase), orbitRadius * std::sin(phase), 0.0},
            inclination,
            ascendingNode);
        const double periapsisSpeed = std::sqrt(
            gravitationalConstant_ * (star.mass + planet.mass) * (1.0 + eccentricity) /
            (semiMajorAxis * (1.0 - eccentricity)));
        planet.velocity = rotateOrbitPlane(
            {-periapsisSpeed * std::sin(phase), periapsisSpeed * std::cos(phase), 0.0},
            inclination,
            ascendingNode);
        planet.trail.clear();
        bodies_.push_back(planet);
    }

    Body ship = makeBody(
        "Nomad Player Ship",
        BodyType::Spacecraft,
        1.0e-12,
        0.090,
        8.0e-8,
        {0.94f, 0.98f, 1.00f, 1.0f},
        {1.08, -0.14, 0.04},
        {0.85, 7.10, 0.08});
    ship.rotationPeriod = 0.0006;
    ship.trail.clear();
    ship.trail.push_back(ship.position);
    bodies_.push_back(ship);
}

double NBodySystem::planetaryInfluenceRadius(std::size_t planetIndex) const {
    if (planetIndex >= bodies_.size()) {
        return 0.0;
    }

    const Body& planet = bodies_[planetIndex];
    const Body* primary = nullptr;
    for (const Body& body : bodies_) {
        if (body.type == BodyType::Star && (&body != &planet) &&
            (primary == nullptr || body.mass > primary->mass)) {
            primary = &body;
        }
    }
    if (primary == nullptr || primary->mass <= 0.0 || planet.mass <= 0.0) {
        return 0.0;
    }

    const double orbitalRadius = length(planet.position - primary->position);
    return orbitalRadius * std::cbrt(planet.mass / (3.0 * primary->mass));
}

void NBodySystem::advanceRotations(double dt) {
    for (Body& body : bodies_) {
        if (body.rotationPeriod <= 1.0e-9 || lengthSquared(body.spinAxis) <= 1.0e-12) {
            continue;
        }
        body.rotationAngle = std::fmod(body.rotationAngle + (2.0 * Pi * dt / body.rotationPeriod), 2.0 * Pi);
        if (body.rotationAngle < 0.0) {
            body.rotationAngle += 2.0 * Pi;
        }
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
        if (scenario_ == Scenario::ProceduralUniverse && body.type != BodyType::Spacecraft) {
            continue;
        }
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

void NBodySystem::detectExplorationEvents() {
    if (scenario_ != Scenario::ProceduralUniverse || elapsedTime_ - lastExplorationEventTime_ < 0.10) {
        return;
    }

    const int nearest = explorerNearestPlanetIndex();
    if (nearest < 0) {
        return;
    }

    const Body& planet = bodies_[static_cast<std::size_t>(nearest)];
    const double distance = explorerNearestPlanetDistance();
    const int ship = firstSpacecraftIndex(bodies_);
    const double contactDistance = planet.physicalRadius +
        (ship >= 0 ? bodies_[static_cast<std::size_t>(ship)].physicalRadius : 0.0);
    if (distance < contactDistance * 4.0) {
        std::ostringstream message;
        message << "physical close approach: " << planet.name << " at " << distance << " AU";
        pushEvent(message.str(), {1.00f, 0.76f, 0.38f, 1.0f});
        lastExplorationEventTime_ = elapsedTime_;
    } else if (distance < planetaryInfluenceRadius(static_cast<std::size_t>(nearest))) {
        std::ostringstream message;
        message << "Hill sphere entry: " << planet.name
                << " net gravity " << explorerLocalGravity() << " AU/yr^2";
        pushEvent(message.str(), {0.58f, 0.92f, 1.00f, 1.0f});
        lastExplorationEventTime_ = elapsedTime_;
    }
}

void NBodySystem::detectGravityAssist() {
    const int craftIndex = firstSpacecraftIndex(bodies_);
    if (craftIndex < 0 || bodies_.size() < 2) {
        return;
    }

    const Body& craft = bodies_[static_cast<std::size_t>(craftIndex)];
    double nearest = std::numeric_limits<double>::infinity();
    std::size_t nearestIndex = 0;
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (static_cast<int>(i) == craftIndex || bodies_[i].type == BodyType::Spacecraft) {
            continue;
        }
        const double distance = length(bodies_[i].position - craft.position);
        if (distance < nearest) {
            nearest = distance;
            nearestIndex = i;
        }
    }

    if (!std::isfinite(nearest)) {
        return;
    }

    spacecraftNearestEncounterDistance_ = std::min(spacecraftNearestEncounterDistance_, nearest);
    const Body& flybyBody = bodies_[nearestIndex];
    const double threshold = std::max(flybyBody.radius * 2.8, 0.11);
    if (nearest < threshold && elapsedTime_ - lastAssistEventTime_ > 0.08) {
        std::ostringstream message;
        message << "gravity assist: " << craft.name << " flyby " << flybyBody.name
                << " at " << nearest << " AU, speed gain "
                << (spacecraftSpeedGain() * 100.0) << "%";
        pushEvent(message.str(), {0.55f, 0.96f, 1.00f, 1.0f});
        lastAssistEventTime_ = elapsedTime_;
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
