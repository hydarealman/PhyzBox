#pragma once

#include "phyz/analysis/analysis.hpp"
#include "phyz/core/units.hpp"
#include "phyz/events/events.hpp"
#include "phyz/integration/integrator.hpp"
#include "phyz/version.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace phyz::engine {

struct SimulationSnapshot {
    std::string libraryVersion = Version;
    UnitSystem units = UnitSystem::astronomical();
    double time = 0.0;
    double fixedStep = 1.0e-4;
    std::uint64_t nextBodyId = 1;
    std::vector<BodyState> bodies;
    std::vector<BodyMetadata> metadata;
};

struct AdvanceReport {
    StepStatus status = StepStatus::Success;
    double requestedEndTime = 0.0;
    double reachedTime = 0.0;
    std::size_t macroSteps = 0;
    std::size_t forceEvaluations = 0;
    std::vector<Event> events;
};

class Simulation {
public:
    explicit Simulation(UnitSystem units = UnitSystem::astronomical());
    Simulation(const Simulation& other);
    Simulation& operator=(const Simulation& other);
    Simulation(Simulation&&) noexcept = default;
    Simulation& operator=(Simulation&&) noexcept = default;

    BodyId add_body(const BodyDefinition& definition);
    bool remove_body(BodyId id);
    [[nodiscard]] BodyState* find_body(BodyId id);
    [[nodiscard]] const BodyState* find_body(BodyId id) const;
    [[nodiscard]] const BodyMetadata* metadata(BodyId id) const;

    [[nodiscard]] std::span<BodyState> bodies() { return bodies_; }
    [[nodiscard]] std::span<const BodyState> bodies() const { return bodies_; }
    [[nodiscard]] const std::vector<BodyMetadata>& all_metadata() const { return metadata_; }
    [[nodiscard]] ForcePipeline& forces() { return forces_; }
    [[nodiscard]] const ForcePipeline& forces() const { return forces_; }

    template <typename IntegratorType, typename... Args>
    IntegratorType& set_integrator(Args&&... args) {
        auto integrator = std::make_unique<IntegratorType>(std::forward<Args>(args)...);
        IntegratorType& reference = *integrator;
        integrator_ = std::move(integrator);
        return reference;
    }

    void add_detector(std::unique_ptr<EventDetector> detector);
    void clear_detectors();
    void set_fixed_step(double step);
    [[nodiscard]] double fixed_step() const { return fixedStep_; }
    [[nodiscard]] double time() const { return time_; }
    [[nodiscard]] const UnitSystem& units() const { return units_; }
    [[nodiscard]] const Integrator& integrator() const { return *integrator_; }

    AdvanceReport step(double dt);
    AdvanceReport advance_to(double endTime);
    [[nodiscard]] InvariantReport invariants() const;

    [[nodiscard]] SimulationSnapshot snapshot() const;
    bool restore(const SimulationSnapshot& snapshot);
    [[nodiscard]] std::string serialize_snapshot() const;
    static std::optional<Simulation> deserialize_snapshot(std::string_view text, std::string* error = nullptr);

private:
    UnitSystem units_;
    double time_ = 0.0;
    double fixedStep_ = 1.0e-4;
    std::uint64_t nextBodyId_ = 1;
    std::vector<BodyState> bodies_;
    std::vector<BodyMetadata> metadata_;
    ForcePipeline forces_;
    std::unique_ptr<Integrator> integrator_;
    std::vector<std::unique_ptr<EventDetector>> detectors_;
    std::vector<BodyState> previousStates_;
};

} // namespace phyz::engine
