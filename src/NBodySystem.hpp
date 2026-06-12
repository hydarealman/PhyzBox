#pragma once

#include "Math3D.hpp"
#include "SimulationConfig.hpp"

#include <deque>
#include <string>
#include <vector>

namespace phyz {

enum class Scenario {
    TrisolarisChaos = 0,
    FigureEight = 1,
    InclinedDance = 2,
    HierarchicalTriple = 3,
    Custom = 4,
};

struct Body {
    std::string name;
    double mass = 1.0;
    double radius = 0.08;
    double physicalRadius = 0.00465;
    Color color{};
    Vec3 position{};
    Vec3 velocity{};
    Vec3 acceleration{};
    std::deque<Vec3> trail{};
};

struct TestParticle {
    Vec3 position{};
    Vec3 velocity{};
    Color color{};
    std::deque<Vec3> trail{};
};

struct EventLogEntry {
    double time = 0.0;
    std::string message;
    Color color{};
};

class NBodySystem {
public:
    NBodySystem();

    void reset(Scenario scenario);
    void reset(const InitialConditionConfig& config);
    void step(double dt);
    void setBodyPosition(std::size_t index, Vec3 position);
    void setBodyVelocity(std::size_t index, Vec3 velocity);
    void rebaselineDiagnostics();
    void seedTestParticles(int count);

    [[nodiscard]] const std::vector<Body>& bodies() const { return bodies_; }
    [[nodiscard]] const std::vector<Body>& shadowBodies() const { return shadowBodies_; }
    [[nodiscard]] const std::vector<TestParticle>& testParticles() const { return testParticles_; }
    [[nodiscard]] const std::deque<EventLogEntry>& events() const { return events_; }
    [[nodiscard]] Scenario scenario() const { return scenario_; }
    [[nodiscard]] const char* scenarioName() const;
    [[nodiscard]] const char* systemStatus() const;
    [[nodiscard]] double time() const { return elapsedTime_; }
    [[nodiscard]] double totalEnergy() const;
    [[nodiscard]] double energyDrift() const;
    [[nodiscard]] Vec3 totalAngularMomentum() const;
    [[nodiscard]] double angularMomentumDrift() const;
    [[nodiscard]] double minSeparation() const;
    [[nodiscard]] double chaosDivergence() const;
    [[nodiscard]] Vec3 centerOfMass() const;
    [[nodiscard]] Vec3 accelerationAt(Vec3 position) const;
    [[nodiscard]] double totalMass() const;
    [[nodiscard]] double softeningLength() const { return softening_; }
    [[nodiscard]] double recommendedTimeStep() const { return recommendedTimeStep_; }
    [[nodiscard]] double recommendedCameraDistance() const { return recommendedCameraDistance_; }
    [[nodiscard]] int mergerCount() const { return mergerCount_; }
    [[nodiscard]] bool collisionMergingEnabled() const { return collisionMergingEnabled_; }

    void setCollisionMergingEnabled(bool enabled) { collisionMergingEnabled_ = enabled; }

private:
    void calculateAccelerations(std::vector<Vec3>& output) const;
    void calculateAccelerationsFor(const std::vector<Body>& source, std::vector<Vec3>& output) const;
    void integrateYoshida4(double dt);
    void integrateLeapfrog(double dt);
    void integrateShadowYoshida4(double dt);
    void integrateShadowLeapfrog(double dt);
    void integrateTestParticles(double dt);
    [[nodiscard]] double adaptiveSubStep(double requestedDt) const;
    void normalizeCenterOfMass();
    void seedTrails();
    void captureTrail(double dt);
    void seedShadowSystem();
    void detectCloseEncounters();
    void resolveCollisions();
    void mergeBodies(std::size_t first, std::size_t second);
    void pushEvent(std::string message, Color color);

    Scenario scenario_ = Scenario::TrisolarisChaos;
    double gravitationalConstant_ = 1.0;
    double softening_ = 0.012;
    double elapsedTime_ = 0.0;
    double trailTimer_ = 0.0;
    double initialEnergy_ = 0.0;
    double initialAngularMomentumMagnitude_ = 0.0;
    double recommendedTimeStep_ = 0.004;
    double recommendedCameraDistance_ = 5.8;
    double lastCloseEventTime_ = -1.0e9;
    int mergerCount_ = 0;
    bool collisionMergingEnabled_ = true;
    std::vector<Body> bodies_;
    std::vector<Body> shadowBodies_;
    std::vector<TestParticle> testParticles_;
    std::vector<Vec3> accelerations_;
    std::vector<Vec3> shadowAccelerations_;
    std::deque<EventLogEntry> events_;
};

} // namespace phyz
