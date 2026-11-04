#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <rtype/layers.hpp>
#include <rtype/position_history.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace rtype {

namespace ecs = aegis2d::ecs;
using aegis2d::Vec2;

namespace {

std::uint32_t id_of(const std::pair<ecs::Entity, Vec2>& enemy) noexcept {
    return enemy.first.id();
}

} // namespace

void PositionHistory::record(std::uint64_t tick, ecs::Registry& registry) {
    if (!frames_.empty() && tick <= frames_.back().tick) {
        frames_.clear();
    }
    Frame frame{.tick = tick, .enemies = {}};
    registry.view<ecs::Collider, ecs::Transform>().each([&frame](ecs::Entity entity,
                                                                 const ecs::Collider& collider,
                                                                 const ecs::Transform& transform) {
        if (collider.layer == layer::enemy) {
            frame.enemies.emplace_back(entity, transform.position);
        }
    });
    std::ranges::sort(frame.enemies, {}, id_of);
    frames_.push_back(std::move(frame));
    while (frames_.size() > capacity) {
        frames_.pop_front();
    }
}

// Ticks recorded one after the other: the frame of a tick is found by its distance to the
// oldest, then checked, as a new game may have left a gap.
const PositionHistory::Frame* PositionHistory::frame(std::uint64_t tick) const noexcept {
    if (frames_.empty() || tick < frames_.front().tick || tick > frames_.back().tick) {
        return nullptr;
    }
    const std::uint64_t offset = tick - frames_.front().tick;
    if (offset < frames_.size() && frames_.at(offset).tick == tick) {
        return &frames_.at(offset);
    }
    const auto found = std::ranges::find(frames_, tick, &Frame::tick);
    return found == frames_.end() ? nullptr : &*found;
}

std::optional<Vec2> PositionHistory::position(ecs::Entity entity, std::uint64_t tick) const {
    const Frame* const kept = frame(tick);
    if (kept == nullptr) {
        return std::nullopt;
    }
    const auto found = std::ranges::lower_bound(kept->enemies, entity.id(), {}, id_of);
    if (found == kept->enemies.end() || found->first != entity) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<std::pair<ecs::Entity, Vec2>> PositionHistory::enemies(std::uint64_t tick) const {
    const Frame* const kept = frame(tick);
    return kept == nullptr ? std::vector<std::pair<ecs::Entity, Vec2>>{} : kept->enemies;
}

std::optional<std::uint64_t> PositionHistory::oldest() const noexcept {
    if (frames_.empty()) {
        return std::nullopt;
    }
    return frames_.front().tick;
}

std::optional<std::uint64_t> PositionHistory::newest() const noexcept {
    if (frames_.empty()) {
        return std::nullopt;
    }
    return frames_.back().tick;
}

std::size_t PositionHistory::size() const noexcept {
    return frames_.size();
}

void PositionHistory::clear() noexcept {
    frames_.clear();
}

} // namespace rtype
