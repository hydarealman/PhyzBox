#pragma once

#include "phyz/core/body.hpp"

#include <span>
#include <memory>
#include <string_view>
#include <vector>

namespace phyz::engine {

enum class EventType {
    Collision,
    CloseApproach,
    EventHorizonCrossing,
    RocheLimitCrossing,
};

struct Event {
    EventType type = EventType::CloseApproach;
    double time = 0.0;
    BodyId primary{};
    BodyId secondary{};
    double value = 0.0;
    double threshold = 0.0;
};

class EventDetector {
public:
    virtual ~EventDetector() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<EventDetector> clone() const = 0;
    virtual void detect(
        std::span<const BodyState> previous,
        std::span<const BodyState> current,
        double startTime,
        double endTime,
        std::vector<Event>& output) const = 0;
};

class CollisionDetector final : public EventDetector {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "physical collision"; }
    [[nodiscard]] std::unique_ptr<EventDetector> clone() const override;
    void detect(
        std::span<const BodyState> previous,
        std::span<const BodyState> current,
        double startTime,
        double endTime,
        std::vector<Event>& output) const override;
};

class CloseApproachDetector final : public EventDetector {
public:
    explicit CloseApproachDetector(double threshold) : threshold_(threshold) {}
    [[nodiscard]] std::string_view name() const noexcept override { return "close approach"; }
    [[nodiscard]] std::unique_ptr<EventDetector> clone() const override;
    void detect(
        std::span<const BodyState> previous,
        std::span<const BodyState> current,
        double startTime,
        double endTime,
        std::vector<Event>& output) const override;

private:
    double threshold_;
};

class EventHorizonDetector final : public EventDetector {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "event horizon crossing"; }
    [[nodiscard]] std::unique_ptr<EventDetector> clone() const override;
    void detect(
        std::span<const BodyState> previous,
        std::span<const BodyState> current,
        double startTime,
        double endTime,
        std::vector<Event>& output) const override;
};

class RocheLimitDetector final : public EventDetector {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Roche limit crossing"; }
    [[nodiscard]] std::unique_ptr<EventDetector> clone() const override;
    void detect(
        std::span<const BodyState> previous,
        std::span<const BodyState> current,
        double startTime,
        double endTime,
        std::vector<Event>& output) const override;
};

} // namespace phyz::engine
