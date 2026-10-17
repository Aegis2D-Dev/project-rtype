#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <rtype/client_world.hpp>
#include <rtype/events.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ratio>
#include <string>
#include <utility>
#include <variant>

namespace rtype {

namespace ecs = aegis2d::ecs;
using aegis2d::Vec2;

namespace {

template <typename... Handlers>
struct Overloaded : Handlers... {
    using Handlers::operator()...;
};

// Size of the sprite of each kind, as the server's tuning gives it; nothing for a kind the
// client does not know (RFC section 4.1).
std::optional<Vec2> size_of(std::uint8_t kind) noexcept {
    switch (static_cast<EntityKind>(kind)) {
    case EntityKind::PlayerShip:
        return tuning::player_size;
    case EntityKind::PlayerMissile:
        return tuning::player_missile_size;
    case EntityKind::BydosSlave:
        return tuning::bydos_size;
    case EntityKind::EnemyMissile:
        return tuning::enemy_missile_size;
    }
    return std::nullopt;
}

bool valid_number(std::uint8_t player) noexcept {
    return player < tuning::max_players;
}

using Ticks = std::chrono::duration<double, std::ratio<1, tuning::tick_rate>>;

// Each later arrival moves the estimate of tick 0 by this part of the difference: the clock
// follows a slower network within a few seconds, but one late datagram barely moves it.
constexpr double drift = 0.02;

} // namespace

void Trail::push(TrailPoint point) noexcept {
    if (count > 0 && point.tick <= points.at(count - 1).tick) {
        return;
    }
    if (count == capacity) {
        for (std::size_t index = 1; index < capacity; ++index) {
            points.at(index - 1) = points.at(index);
        }
        --count;
    }
    points.at(count) = point;
    ++count;
}

Vec2 interpolate(const Trail& trail, double tick) noexcept {
    if (trail.count == 0) {
        return {};
    }
    const TrailPoint& oldest = trail.points.front();
    if (tick <= static_cast<double>(oldest.tick)) {
        return oldest.position;
    }
    for (std::size_t index = 1; index < trail.count; ++index) {
        const TrailPoint& before = trail.points.at(index - 1);
        const TrailPoint& after = trail.points.at(index);
        if (tick <= static_cast<double>(after.tick)) {
            const auto part = static_cast<float>((tick - static_cast<double>(before.tick)) /
                                                 static_cast<double>(after.tick - before.tick));
            return before.position + ((after.position - before.position) * part);
        }
    }
    return trail.points.at(trail.count - 1).position;
}

void ClientWorld::apply(const GameEvent& event, Clock::time_point now) {
    const bool applied = std::visit(
        Overloaded{
            [this](const SpawnEvent& spawned) { return spawn(spawned); },
            [this, now](const DestroyEvent& destroyed) { return destroy(destroyed, now); },
            [this](const DeathEvent& death) {
                if (!valid_number(death.player)) {
                    return false;
                }
                player(death.player).lives = death.lives;
                return true;
            },
            [this](const PlayerJoinedEvent& joined) {
                if (!valid_number(joined.player)) {
                    return false;
                }
                player(joined.player).name = joined.name;
                return true;
            },
            [this](const PlayerLeftEvent& left) {
                if (!valid_number(left.player)) {
                    return false;
                }
                players_.at(left.player).reset();
                return true;
            },
        },
        event);
    if (applied) {
        applied_.push_back(event);
    }
}

bool ClientWorld::spawn(const SpawnEvent& event) {
    const std::optional<Vec2> size = size_of(event.kind);
    if (!size.has_value()) {
        return false;
    }
    Known& known = known_[event.id];
    // RFC section 4.5: older than what was applied to this id (a late Spawn after its
    // Destroy, or a duplicate): ignored.
    if ((known.entity.has_value() || known.destroyed) && event.tick < known.event_tick) {
        return false;
    }
    if (known.entity.has_value()) {
        registry_.destroy(known.entity.value_or(ecs::Entity{}));
    }
    const ecs::Entity entity = registry_.create();
    registry_.emplace<ecs::Transform>(entity, ecs::Transform{.position = {event.x, event.y}});
    registry_.emplace<ecs::Velocity>(entity, ecs::Velocity{.value = {event.vx, event.vy}});
    registry_.emplace<ecs::Sprite>(entity, ecs::Sprite{.size = size.value_or(Vec2{})});
    registry_.emplace<ecs::NetworkId>(entity, ecs::NetworkId{.value = event.id});
    registry_.emplace<Replicated>(
        entity, Replicated{.id = event.id, .kind = event.kind, .owner = event.owner});
    Trail trail;
    trail.push(TrailPoint{.tick = event.tick, .position = {event.x, event.y}});
    registry_.emplace<Trail>(entity, trail);
    known = Known{.entity = entity,
                  .event_tick = event.tick,
                  .record_tick = event.tick,
                  .destroyed = false,
                  .destroyed_at = {}};
    return true;
}

bool ClientWorld::destroy(const DestroyEvent& event, Clock::time_point now) {
    Known& known = known_[event.id];
    if ((known.entity.has_value() || known.destroyed) && event.tick < known.event_tick) {
        return false;
    }
    if (known.entity.has_value()) {
        registry_.destroy(known.entity.value_or(ecs::Entity{}));
    }
    // Remembered even if its Spawn never came: a late Spawn is then ignored.
    known = Known{.entity = std::nullopt,
                  .event_tick = event.tick,
                  .record_tick = event.tick,
                  .destroyed = true,
                  .destroyed_at = now};
    return true;
}

void ClientWorld::apply(const Snapshot& snapshot) {
    for (const PlayerRecord& record : snapshot.players) {
        if (!valid_number(record.player)) {
            continue;
        }
        RemotePlayer& updated = player(record.player);
        updated.ship = record.ship;
        updated.lives = record.lives;
        updated.score = record.score;
    }
    for (const EntityRecord& record : snapshot.entities) {
        const auto found = known_.find(record.id);
        // RFC section 4.3: a snapshot never creates an entity; section 4.5: nor updates a
        // destroyed one, nor goes back in time.
        if (found == known_.end() || !found->second.entity.has_value() ||
            snapshot.tick < found->second.record_tick || !size_of(record.kind).has_value()) {
            continue;
        }
        Known& known = found->second;
        const ecs::Entity entity = known.entity.value_or(ecs::Entity{});
        registry_.get<ecs::Transform>(entity).position = {record.x, record.y};
        registry_.get<ecs::Velocity>(entity).value = {record.vx, record.vy};
        registry_.get<ecs::Sprite>(entity).frame = record.frame;
        registry_.get<Trail>(entity).push(
            TrailPoint{.tick = snapshot.tick, .position = {record.x, record.y}});
        known.record_tick = snapshot.tick;
    }
}

void ClientWorld::apply(const Snapshot& snapshot, Clock::time_point received) {
    apply(snapshot);
    const auto sent =
        std::chrono::duration_cast<Clock::duration>(Ticks{static_cast<double>(snapshot.tick)});
    const Clock::time_point zero = received - sent;
    if (!tick_zero_.has_value() || zero < *tick_zero_) {
        tick_zero_ = zero;
    } else {
        const auto late = std::chrono::duration<double>{zero - *tick_zero_};
        tick_zero_ = *tick_zero_ + std::chrono::duration_cast<Clock::duration>(late * drift);
    }
}

std::optional<double> ClientWorld::server_tick(Clock::time_point now) const {
    if (!tick_zero_.has_value()) {
        return std::nullopt;
    }
    return std::chrono::duration_cast<Ticks>(now - *tick_zero_).count();
}

std::optional<double> ClientWorld::render_tick(Clock::time_point now) const {
    return server_tick(now - interpolation_delay);
}

Vec2 ClientWorld::position_at(ecs::Entity entity, double tick) const {
    if (const auto trail = registry_.try_get<Trail>(entity)) {
        return interpolate(trail->get(), tick);
    }
    if (const auto transform = registry_.try_get<ecs::Transform>(entity)) {
        return transform->get().position;
    }
    return {};
}

std::vector<GameEvent> ClientWorld::take_events() {
    std::vector<GameEvent> events;
    events.swap(applied_);
    return events;
}

void ClientWorld::join(std::uint8_t player, std::string name) {
    if (valid_number(player)) {
        this->player(player).name = std::move(name);
    }
}

void ClientWorld::prune(Clock::time_point now) {
    std::erase_if(known_, [now](const auto& entry) {
        return entry.second.destroyed && now - entry.second.destroyed_at >= forget_after;
    });
}

std::optional<ecs::Entity> ClientWorld::entity(std::uint32_t id) const {
    const auto found = known_.find(id);
    if (found == known_.end()) {
        return std::nullopt;
    }
    return found->second.entity;
}

std::size_t ClientWorld::entities() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(
        known_, [](const auto& entry) { return entry.second.entity.has_value(); }));
}

const std::array<std::optional<RemotePlayer>, tuning::max_players>&
ClientWorld::players() const noexcept {
    return players_;
}

RemotePlayer& ClientWorld::player(std::uint8_t number) {
    std::optional<RemotePlayer>& slot = players_.at(number);
    if (!slot.has_value()) {
        return slot.emplace();
    }
    return *slot;
}

ecs::Registry& ClientWorld::registry() noexcept {
    return registry_;
}

const ecs::Registry& ClientWorld::registry() const noexcept {
    return registry_;
}

} // namespace rtype
