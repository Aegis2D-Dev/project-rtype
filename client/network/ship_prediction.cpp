#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/sequence.hpp>

#include <rtype/input.hpp>
#include <rtype/ship_movement.hpp>
#include <rtype/ship_prediction.hpp>

#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>

namespace rtype {

namespace net = aegis2d::net;
using aegis2d::Vec2;

void ShipPrediction::start(std::uint32_t ship, Vec2 position) {
    ship_ = ship;
    position_ = position;
    pending_.clear();
}

void ShipPrediction::stop() noexcept {
    ship_.reset();
    pending_.clear();
}

void ShipPrediction::apply(const InputRecord& input, float delta) {
    if (!ship_.has_value()) {
        return;
    }
    position_ =
        step_ship(position_, aegis2d::ecs::PlayerInput{.held = input.actions}, delta).position;
    pending_.push_back(input);
    if (pending_.size() > max_pending) {
        pending_.pop_front();
    }
}

void ShipPrediction::acknowledge(std::uint16_t sequence) {
    while (!pending_.empty() && !net::sequence_greater_than(pending_.front().sequence, sequence)) {
        pending_.pop_front();
    }
}

float ShipPrediction::reconcile(Vec2 server, std::optional<std::uint16_t> acknowledged,
                                float delta) {
    if (!ship_.has_value()) {
        return 0.0F;
    }
    if (acknowledged.has_value()) {
        acknowledge(*acknowledged);
    }
    const Vec2 before = position_;
    position_ = server;
    for (const InputRecord& input : pending_) {
        position_ =
            step_ship(position_, aegis2d::ecs::PlayerInput{.held = input.actions}, delta).position;
    }
    const Vec2 moved = position_ - before;
    last_correction_ = std::hypot(moved.x, moved.y);
    if (last_correction_ > correction_threshold) {
        ++corrections_;
    }
    return last_correction_;
}

std::uint64_t ShipPrediction::corrections() const noexcept {
    return corrections_;
}

float ShipPrediction::last_correction() const noexcept {
    return last_correction_;
}

std::optional<std::uint32_t> ShipPrediction::ship() const noexcept {
    return ship_;
}

std::optional<Vec2> ShipPrediction::position() const noexcept {
    if (!ship_.has_value()) {
        return std::nullopt;
    }
    return position_;
}

const std::deque<InputRecord>& ShipPrediction::pending() const noexcept {
    return pending_;
}

} // namespace rtype
