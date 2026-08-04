#pragma once

#include "phyz/dynamics/force_model.hpp"

#include <memory>
#include <span>
#include <string_view>

namespace phyz::engine {

struct IntegratorTraits {
    int formalOrder = 1;
    bool symplectic = false;
    bool adaptive = false;
    bool supportsVelocityDependentForces = false;
};

enum class StepStatus {
    Success,
    InvalidTimeStep,
    NonFiniteState,
    IncompatibleForce,
};

struct StepResult {
    StepStatus status = StepStatus::Success;
    double advancedTime = 0.0;
    double smallestStep = 0.0;
    std::size_t forceEvaluations = 0;
    std::size_t acceptedSteps = 0;
    std::size_t rejectedSteps = 0;
    double estimatedError = 0.0;
};

class Integrator {
public:
    virtual ~Integrator() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual IntegratorTraits traits() const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<Integrator> clone() const = 0;
    virtual StepResult step(
        std::span<BodyState> bodies,
        const ForcePipeline& forces,
        double time,
        double dt) = 0;
};

class Leapfrog2Fixed final : public Integrator {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Leapfrog-2 fixed"; }
    [[nodiscard]] IntegratorTraits traits() const noexcept override { return {2, true, false, false}; }
    [[nodiscard]] std::unique_ptr<Integrator> clone() const override;
    StepResult step(std::span<BodyState> bodies, const ForcePipeline& forces, double time, double dt) override;

private:
    std::vector<Vec3d> accelerations_;
};

class Yoshida4Fixed final : public Integrator {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Yoshida-4 fixed"; }
    [[nodiscard]] IntegratorTraits traits() const noexcept override { return {4, true, false, false}; }
    [[nodiscard]] std::unique_ptr<Integrator> clone() const override;
    StepResult step(std::span<BodyState> bodies, const ForcePipeline& forces, double time, double dt) override;

private:
    Leapfrog2Fixed leapfrog_;
};

} // namespace phyz::engine
