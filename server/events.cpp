#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/damage.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/lives.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/net/binary.hpp>

#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/login.hpp>
#include <rtype/server_game.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

namespace ecs = aegis2d::ecs;
namespace net = aegis2d::net;

namespace {

template <typename... Handlers>
struct Overloaded : Handlers... {
    using Handlers::operator()...;
};

void write_spawn(net::BinaryWriter& writer, const SpawnEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::Spawn));
    writer.write_u32(event.tick);
    writer.write_u32(event.id);
    writer.write_u8(event.kind);
    writer.write_f32(event.x);
    writer.write_f32(event.y);
    writer.write_f32(event.vx);
    writer.write_f32(event.vy);
    writer.write_u8(event.owner);
}

void write_destroy(net::BinaryWriter& writer, const DestroyEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::Destroy));
    writer.write_u32(event.tick);
    writer.write_u32(event.id);
    writer.write_u8(static_cast<std::uint8_t>(event.reason));
    writer.write_f32(event.x);
    writer.write_f32(event.y);
}

void write_death(net::BinaryWriter& writer, const DeathEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::Death));
    writer.write_u32(event.tick);
    writer.write_u8(event.player);
    writer.write_u8(event.lives);
    writer.write_f32(event.x);
    writer.write_f32(event.y);
}

void write_player_joined(net::BinaryWriter& writer, const PlayerJoinedEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::PlayerJoined));
    writer.write_u32(event.tick);
    writer.write_u8(event.player);
    writer.write_string(event.name);
}

void write_player_left(net::BinaryWriter& writer, const PlayerLeftEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::PlayerLeft));
    writer.write_u32(event.tick);
    writer.write_u8(event.player);
    writer.write_u8(static_cast<std::uint8_t>(event.reason));
}

bool finite(std::initializer_list<float> values) {
    return std::ranges::all_of(values, [](float value) { return std::isfinite(value); });
}

bool valid_player(std::uint8_t player) noexcept {
    return player < tuning::max_players;
}

std::optional<GameEvent> read_spawn(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint32_t> id = reader.read_u32();
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<float> x = reader.read_f32();
    const std::optional<float> y = reader.read_f32();
    const std::optional<float> vx = reader.read_f32();
    const std::optional<float> vy = reader.read_f32();
    const std::optional<std::uint8_t> owner = reader.read_u8();
    if (!id || !kind || !x || !y || !vx || !vy || !owner || !reader.at_end() ||
        !finite({*x, *y, *vx, *vy}) || (!valid_player(*owner) && *owner != no_player)) {
        return std::nullopt;
    }
    return SpawnEvent{.tick = tick,
                      .id = *id,
                      .kind = *kind,
                      .x = *x,
                      .y = *y,
                      .vx = *vx,
                      .vy = *vy,
                      .owner = *owner};
}

std::optional<GameEvent> read_destroy(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint32_t> id = reader.read_u32();
    const std::optional<std::uint8_t> reason = reader.read_u8();
    const std::optional<float> x = reader.read_f32();
    const std::optional<float> y = reader.read_f32();
    if (!id || !reason || !x || !y || !reader.at_end() || !finite({*x, *y})) {
        return std::nullopt;
    }
    // An unknown reason is kept: the client reads it as Killed (RFC section 4.4).
    return DestroyEvent{
        .tick = tick, .id = *id, .reason = static_cast<DestroyReason>(*reason), .x = *x, .y = *y};
}

std::optional<GameEvent> read_death(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint8_t> player = reader.read_u8();
    const std::optional<std::uint8_t> lives = reader.read_u8();
    const std::optional<float> x = reader.read_f32();
    const std::optional<float> y = reader.read_f32();
    if (!player || !lives || !x || !y || !reader.at_end() || !valid_player(*player) ||
        !finite({*x, *y})) {
        return std::nullopt;
    }
    return DeathEvent{.tick = tick, .player = *player, .lives = *lives, .x = *x, .y = *y};
}

// The name follows the rules of LoginRequest (RFC section 6.4).
std::optional<GameEvent> read_player_joined(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint8_t> player = reader.read_u8();
    std::optional<std::string> name = reader.read_string();
    if (!player || !name || !reader.at_end() || !valid_player(*player) ||
        !valid_player_name(*name)) {
        return std::nullopt;
    }
    return PlayerJoinedEvent{.tick = tick, .player = *player, .name = std::move(*name)};
}

std::optional<GameEvent> read_player_left(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint8_t> player = reader.read_u8();
    const std::optional<std::uint8_t> reason = reader.read_u8();
    if (!player || !reason || !reader.at_end() || !valid_player(*player)) {
        return std::nullopt;
    }
    // An unknown reason is kept: the client reads it as Left (RFC section 4.4).
    return PlayerLeftEvent{
        .tick = tick, .player = *player, .reason = static_cast<LeaveReason>(*reason)};
}

// Player number of a ship of the game, or no_player.
std::uint8_t player_of(const GameInstance& instance, ecs::Entity ship) {
    for (std::uint8_t player = 0; player < tuning::max_players; ++player) {
        if (instance.ship(player) == ship) {
            return player;
        }
    }
    return no_player;
}

// A ship belongs to its player, a missile to its shooter's player, an enemy to nobody.
std::uint8_t owner_of(GameInstance& instance, ecs::Entity entity, EntityKind kind) {
    switch (kind) {
    case EntityKind::PlayerShip:
        return player_of(instance, entity);
    case EntityKind::PlayerMissile: {
        const auto owner = instance.game().registry().try_get<ecs::Owner>(entity);
        return owner ? player_of(instance, owner->get().entity) : no_player;
    }
    case EntityKind::BydosSlave:
    case EntityKind::EnemyMissile:
        break;
    }
    return no_player;
}

aegis2d::Vec2 velocity_of(const ecs::Registry& world, ecs::Entity entity) {
    const auto velocity = world.try_get<ecs::Velocity>(entity);
    return velocity ? velocity->get().value : aegis2d::Vec2{};
}

bool listed(std::span<const ecs::Entity> entities, ecs::Entity entity) {
    return std::ranges::find(entities, entity) != entities.end();
}

// Why an entity of the last tick is gone, from the tick's reports.
DestroyReason reason_of(const ServerGame& game, ecs::Entity entity, EntityKind kind) {
    const ecs::DamageReport& damage = game.damage();
    if (std::ranges::any_of(damage.kills,
                            [&](const ecs::Kill& kill) { return kill.victim == entity; })) {
        return DestroyReason::Killed;
    }
    if (listed(damage.consumed, entity)) {
        return DestroyReason::Consumed;
    }
    if (listed(game.despawned(), entity)) {
        return DestroyReason::Despawned;
    }
    // Not destroyed by the game: removed by GameInstance::leave() for a ship.
    return kind == EntityKind::PlayerShip ? DestroyReason::PlayerLeft : DestroyReason::Despawned;
}

std::uint32_t tick_of(GameInstance& instance) {
    return static_cast<std::uint32_t>(instance.game().ticks());
}

std::uint8_t clamp_lives(std::uint32_t lives) {
    return static_cast<std::uint8_t>(std::min<std::uint32_t>(lives, 0xFF));
}

} // namespace

std::vector<std::byte> write_event(const GameEvent& event) {
    net::BinaryWriter writer;
    std::visit(
        Overloaded{[&](const SpawnEvent& spawn) { write_spawn(writer, spawn); },
                   [&](const DestroyEvent& destroy) { write_destroy(writer, destroy); },
                   [&](const DeathEvent& death) { write_death(writer, death); },
                   [&](const PlayerJoinedEvent& joined) { write_player_joined(writer, joined); },
                   [&](const PlayerLeftEvent& left) { write_player_left(writer, left); }},
        event);
    return {writer.data().begin(), writer.data().end()};
}

std::vector<std::vector<std::byte>> write_events(std::span<const GameEvent> events) {
    std::vector<std::vector<std::byte>> encoded;
    encoded.reserve(events.size());
    for (const GameEvent& event : events) {
        encoded.push_back(write_event(event));
    }
    return encoded;
}

std::optional<GameEvent> read_event(std::span<const std::byte> bytes) {
    net::BinaryReader reader{bytes};
    const std::optional<std::uint8_t> type = reader.read_u8();
    const std::optional<std::uint32_t> tick = reader.read_u32();
    if (!type || !tick) {
        return std::nullopt;
    }
    switch (static_cast<EventType>(*type)) {
    case EventType::Spawn:
        return read_spawn(reader, *tick);
    case EventType::Destroy:
        return read_destroy(reader, *tick);
    case EventType::Death:
        return read_death(reader, *tick);
    case EventType::PlayerJoined:
        return read_player_joined(reader, *tick);
    case EventType::PlayerLeft:
        return read_player_left(reader, *tick);
    case EventType::GameOver:
        break;
    }
    return std::nullopt;
}

void EventTracker::follow(GameInstance& instance) {
    // A new game: the entities of the old one are gone with it, and nobody saw them go.
    if (instance.games() != game_) {
        entities_.clear();
        game_ = instance.games();
    }
}

std::vector<GameEvent> EventTracker::update(GameInstance& instance) {
    follow(instance);
    const ServerGame& game = instance.game();
    ecs::Registry& world = instance.game().registry();
    const std::uint32_t tick = tick_of(instance);
    std::vector<GameEvent> events;

    // The replicated entities now: the ones already known keep their owner.
    std::unordered_map<ecs::Entity, Tracked> current;
    world.view<ecs::NetworkId, ecs::Transform, ecs::Collider>().each(
        [&](ecs::Entity entity, const ecs::NetworkId& id, const ecs::Transform& transform,
            const ecs::Collider& collider) {
            const std::optional<EntityKind> kind = kind_of_layer(collider.layer);
            if (!kind) {
                return;
            }
            const auto known = entities_.find(entity);
            Tracked tracked{.id = id.value,
                            .kind = *kind,
                            .x = transform.position.x,
                            .y = transform.position.y,
                            .owner = known != entities_.end() ? known->second.owner
                                                              : owner_of(instance, entity, *kind)};
            if (known == entities_.end()) {
                const aegis2d::Vec2 velocity = velocity_of(world, entity);
                events.emplace_back(SpawnEvent{.tick = tick,
                                               .id = tracked.id,
                                               .kind = static_cast<std::uint8_t>(*kind),
                                               .x = tracked.x,
                                               .y = tracked.y,
                                               .vx = velocity.x,
                                               .vy = velocity.y,
                                               .owner = tracked.owner});
            }
            current.emplace(entity, tracked);
        });

    // Where the ship was before the hit: a ship with lives left is back at its spawn point.
    for (const ecs::LifeLost& death : game.lives_lost()) {
        const auto known = entities_.find(death.player);
        if (known == entities_.end() || known->second.owner == no_player) {
            continue;
        }
        events.emplace_back(DeathEvent{.tick = tick,
                                       .player = known->second.owner,
                                       .lives = clamp_lives(death.lives_left),
                                       .x = known->second.x,
                                       .y = known->second.y});
    }

    for (const auto& [entity, tracked] : entities_) {
        if (current.contains(entity)) {
            continue;
        }
        events.emplace_back(DestroyEvent{.tick = tick,
                                         .id = tracked.id,
                                         .reason = reason_of(game, entity, tracked.kind),
                                         .x = tracked.x,
                                         .y = tracked.y});
    }
    entities_ = std::move(current);
    return events;
}

std::vector<GameEvent> EventTracker::spawns(GameInstance& instance) {
    follow(instance);
    const ecs::Registry& world = instance.game().registry();
    const std::uint32_t tick = tick_of(instance);
    std::vector<GameEvent> events;
    for (const auto& [entity, tracked] : entities_) {
        // Removed by a command since the last tick, like the ship of a player who left.
        const auto transform = world.try_get<ecs::Transform>(entity);
        if (!world.alive(entity) || !transform) {
            continue;
        }
        const aegis2d::Vec2 position = transform->get().position;
        const aegis2d::Vec2 velocity = velocity_of(world, entity);
        events.emplace_back(SpawnEvent{.tick = tick,
                                       .id = tracked.id,
                                       .kind = static_cast<std::uint8_t>(tracked.kind),
                                       .x = position.x,
                                       .y = position.y,
                                       .vx = velocity.x,
                                       .vy = velocity.y,
                                       .owner = tracked.owner});
    }
    return events;
}

std::size_t EventTracker::tracked() const noexcept {
    return entities_.size();
}

} // namespace rtype
