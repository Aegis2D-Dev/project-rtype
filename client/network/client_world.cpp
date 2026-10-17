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
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
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

} // namespace

void ClientWorld::apply(const GameEvent& event, Clock::time_point now) {
    std::visit(Overloaded{
                   [this](const SpawnEvent& spawned) { spawn(spawned); },
                   [this, now](const DestroyEvent& destroyed) { destroy(destroyed, now); },
                   [this](const DeathEvent& death) {
                       if (valid_number(death.player)) {
                           player(death.player).lives = death.lives;
                       }
                   },
                   [this](const PlayerJoinedEvent& joined) {
                       if (valid_number(joined.player)) {
                           player(joined.player).name = joined.name;
                       }
                   },
                   [this](const PlayerLeftEvent& left) {
                       if (valid_number(left.player)) {
                           players_.at(left.player).reset();
                       }
                   },
               },
               event);
}

void ClientWorld::spawn(const SpawnEvent& event) {
    const std::optional<Vec2> size = size_of(event.kind);
    if (!size.has_value()) {
        return;
    }
    Known& known = known_[event.id];
    // RFC section 4.5: older than what was applied to this id (a late Spawn after its
    // Destroy, or a duplicate): ignored.
    if ((known.entity.has_value() || known.destroyed) && event.tick < known.event_tick) {
        return;
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
    known = Known{.entity = entity,
                  .event_tick = event.tick,
                  .record_tick = event.tick,
                  .destroyed = false,
                  .destroyed_at = {}};
}

void ClientWorld::destroy(const DestroyEvent& event, Clock::time_point now) {
    Known& known = known_[event.id];
    if ((known.entity.has_value() || known.destroyed) && event.tick < known.event_tick) {
        return;
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
        known.record_tick = snapshot.tick;
    }
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
