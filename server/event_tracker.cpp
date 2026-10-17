#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/damage.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/lives.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <rtype/event_tracker.hpp>
#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/server_game.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>
#include <rtype/world_snapshot.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rtype {

namespace ecs = aegis2d::ecs;

namespace {

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
