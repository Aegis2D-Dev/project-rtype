#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <rtype/game_instance.hpp>
#include <rtype/layers.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>
#include <rtype/world_snapshot.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>

namespace rtype {

namespace ecs = aegis2d::ecs;

namespace {

// A component of an entity, or nullptr.
template <typename Component>
const Component* find(const ecs::Registry& world, ecs::Entity entity) {
    const auto component = world.try_get<Component>(entity);
    return component.has_value() ? &component->get() : nullptr;
}

template <typename T>
std::uint8_t clamp_u8(T value) noexcept {
    return static_cast<std::uint8_t>(std::min<T>(value, T{0xFF}));
}

} // namespace

// The kind follows the collision layer: each layer holds one kind of entity for now.
std::optional<EntityKind> kind_of_layer(std::uint32_t layer) noexcept {
    switch (layer) {
    case layer::player:
        return EntityKind::PlayerShip;
    case layer::player_missile:
        return EntityKind::PlayerMissile;
    case layer::enemy:
        return EntityKind::BydosSlave;
    case layer::enemy_missile:
        return EntityKind::EnemyMissile;
    default:
        return std::nullopt;
    }
}

Snapshot take_snapshot(GameInstance& instance) {
    ecs::Registry& world = instance.game().registry();
    Snapshot snapshot{
        .tick = static_cast<std::uint32_t>(instance.game().ticks()), .players = {}, .entities = {}};
    for (std::uint8_t player = 0; player < tuning::max_players; ++player) {
        const std::optional<PlayerState> state = instance.player_state(player);
        if (!state) {
            continue;
        }
        snapshot.players.push_back(PlayerRecord{
            .player = player,
            .ship = state->ship ? state->ship->id() : no_entity,
            .lives = clamp_u8(state->lives),
            .score = state->score,
        });
    }
    world.view<ecs::NetworkId, ecs::Transform, ecs::Collider>().each(
        [&](ecs::Entity entity, const ecs::NetworkId& id, const ecs::Transform& transform,
            const ecs::Collider& collider) {
            const std::optional<EntityKind> kind = kind_of_layer(collider.layer);
            if (!kind) {
                return;
            }
            const auto* const velocity = find<ecs::Velocity>(world, entity);
            const auto* const sprite = find<ecs::Sprite>(world, entity);
            snapshot.entities.push_back(EntityRecord{
                .id = id.value,
                .kind = static_cast<std::uint8_t>(*kind),
                .x = transform.position.x,
                .y = transform.position.y,
                .vx = velocity != nullptr ? velocity->value.x : 0.0F,
                .vy = velocity != nullptr ? velocity->value.y : 0.0F,
                .frame = sprite != nullptr ? clamp_u8(sprite->frame) : std::uint8_t{0},
            });
        });
    return snapshot;
}

} // namespace rtype
