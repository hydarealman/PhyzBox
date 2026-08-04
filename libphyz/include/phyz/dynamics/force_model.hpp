#pragma once

#include "phyz/core/body.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace phyz::engine {

struct ForceTraits {
    bool conservative = true;
    bool velocityDependent = false;
    bool timeDependent = false;
    bool translationInvariant = true;
    bool reversible = true;
};

class ForceModel {
public:
    virtual ~ForceModel() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual ForceTraits traits() const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<ForceModel> clone() const = 0;

    virtual void accumulate(
        std::span<const BodyState> bodies,
        double time,
        std::span<Vec3d> accelerations) const = 0;

    [[nodiscard]] virtual std::optional<double> potential_energy(
        std::span<const BodyState> bodies,
        double time) const = 0;
};

class ForcePipeline {
public:
    ForcePipeline() = default;
    ForcePipeline(const ForcePipeline& other);
    ForcePipeline& operator=(const ForcePipeline& other);
    ForcePipeline(ForcePipeline&&) noexcept = default;
    ForcePipeline& operator=(ForcePipeline&&) noexcept = default;

    template <typename Model, typename... Args>
    Model& add(Args&&... args) {
        auto model = std::make_unique<Model>(std::forward<Args>(args)...);
        Model& reference = *model;
        models_.push_back(std::move(model));
        return reference;
    }

    void add(std::unique_ptr<ForceModel> model);
    void clear();
    void calculate(std::span<const BodyState> bodies, double time, std::span<Vec3d> output) const;
    [[nodiscard]] std::optional<double> potential_energy(std::span<const BodyState> bodies, double time) const;
    [[nodiscard]] bool is_conservative() const;
    [[nodiscard]] bool is_velocity_dependent() const;
    [[nodiscard]] const std::vector<std::unique_ptr<ForceModel>>& models() const { return models_; }

private:
    std::vector<std::unique_ptr<ForceModel>> models_;
};

enum class GravityLaw {
    Newtonian,
    PlummerSoftened,
    PaczynskiWiita,
};

class PairwiseGravity final : public ForceModel {
public:
    PairwiseGravity(double gravitationalConstant, GravityLaw law = GravityLaw::Newtonian, double softening = 0.0);

    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] ForceTraits traits() const noexcept override { return {}; }
    [[nodiscard]] std::unique_ptr<ForceModel> clone() const override;
    void accumulate(std::span<const BodyState> bodies, double time, std::span<Vec3d> accelerations) const override;
    [[nodiscard]] std::optional<double> potential_energy(std::span<const BodyState> bodies, double time) const override;

    [[nodiscard]] double gravitational_constant() const { return gravitationalConstant_; }
    [[nodiscard]] double softening() const { return softening_; }
    [[nodiscard]] GravityLaw law() const { return law_; }

private:
    double gravitationalConstant_;
    GravityLaw law_;
    double softening_;
};

class ConstantAcceleration final : public ForceModel {
public:
    ConstantAcceleration(BodyId target, Vec3d acceleration);

    [[nodiscard]] std::string_view name() const noexcept override { return "constant acceleration"; }
    [[nodiscard]] ForceTraits traits() const noexcept override { return {false, false, false, false, true}; }
    [[nodiscard]] std::unique_ptr<ForceModel> clone() const override;
    void accumulate(std::span<const BodyState> bodies, double time, std::span<Vec3d> accelerations) const override;
    [[nodiscard]] std::optional<double> potential_energy(std::span<const BodyState>, double) const override {
        return std::nullopt;
    }

    void set_acceleration(Vec3d acceleration) { acceleration_ = acceleration; }
    [[nodiscard]] BodyId target() const { return target_; }
    [[nodiscard]] Vec3d acceleration() const { return acceleration_; }

private:
    BodyId target_;
    Vec3d acceleration_;
};

} // namespace phyz::engine
