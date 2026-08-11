#pragma once

#include "phyz/libphyz.hpp"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <optional>

namespace godot {

class PhyzSimulation : public Node {
    GDCLASS(PhyzSimulation, Node)

public:
    PhyzSimulation();

    void load_mission(int missionIndex);
    Dictionary advance(double years);
    Dictionary advance_controlled(double years, double x, double y, double z, double throttle);
    bool apply_impulse(std::int64_t bodyId, double x, double y, double z);
    bool schedule_impulse(std::int64_t bodyId, double time, double x, double y, double z);
    bool cancel_scheduled_impulse();
    Dictionary get_scheduled_impulse() const;
    Array predict(std::int64_t bodyId, double x, double y, double z, double burnTime, double duration, double samplePeriod) const;
    Array get_bodies() const;
    Dictionary get_invariants() const;
    Dictionary get_orbital_elements(std::int64_t bodyId, std::int64_t primaryId) const;
    Dictionary get_mission_definition() const;
    Dictionary evaluate_mission() const;
    String save_snapshot() const;
    bool load_snapshot(const String& text);
    double get_time() const;
    double get_fixed_step() const;
    String get_integrator_name() const;

protected:
    static void _bind_methods();

private:
    void build_two_body_transfer();
    void build_rendezvous();
    void build_gravity_assist();
    void build_asteroid_deflection();
    void build_chaos_survival();
    void add_gravity(double softening = 1.0e-6);
    void attach_thrust_force();
    static phyz::engine::BodyKind kind_from_int(int value);

    phyz::engine::Simulation simulation_;
    int missionIndex_ = 0;
    std::int64_t playerBodyId_ = 0;
    std::int64_t targetBodyId_ = 0;
    std::int64_t primaryBodyId_ = 0;
    double deltaVBudget_ = 1.0;
    double deltaVSpent_ = 0.0;
    double missionDeadline_ = 2.0;
    double initialPlayerSpeed_ = 0.0;
    double maxThrustAcceleration_ = 5.0;
    phyz::engine::ConstantAcceleration* thrustForce_ = nullptr;
    std::optional<phyz::engine::ImpulseManeuver> scheduledImpulse_;
};

} // namespace godot
