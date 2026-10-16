#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/net/binary.hpp>

#include <rtype/game_instance.hpp>
#include <rtype/layers.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtype {

namespace ecs = aegis2d::ecs;
namespace net = aegis2d::net;

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

void write_records(net::BinaryWriter& writer, const Snapshot& snapshot,
                   std::span<const EntityRecord> entities) {
    writer.write_u32(snapshot.tick);
    writer.write_u8(static_cast<std::uint8_t>(snapshot.players.size()));
    for (const PlayerRecord& player : snapshot.players) {
        writer.write_u8(player.player);
        writer.write_u32(player.ship);
        writer.write_u8(player.lives);
        writer.write_u32(player.score);
    }
    writer.write_u16(static_cast<std::uint16_t>(entities.size()));
    for (const EntityRecord& entity : entities) {
        writer.write_u32(entity.id);
        writer.write_u8(entity.kind);
        writer.write_f32(entity.x);
        writer.write_f32(entity.y);
        writer.write_f32(entity.vx);
        writer.write_f32(entity.vy);
        writer.write_u8(entity.frame);
    }
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

std::vector<std::vector<std::byte>> write_snapshot(const Snapshot& snapshot) {
    const std::span<const EntityRecord> entities{snapshot.entities};
    // At least one payload, so a world with no entity still sends its players.
    const std::size_t count = std::max<std::size_t>(
        1, (entities.size() + max_entities_per_datagram - 1) / max_entities_per_datagram);
    std::vector<std::vector<std::byte>> payloads;
    payloads.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t first = index * max_entities_per_datagram;
        const std::size_t size = std::min(max_entities_per_datagram, entities.size() - first);
        net::BinaryWriter writer;
        write_records(writer, snapshot, entities.subspan(first, size));
        payloads.emplace_back(writer.data().begin(), writer.data().end());
    }
    return payloads;
}

std::optional<Snapshot> read_snapshot(std::span<const std::byte> payload) {
    net::BinaryReader reader{payload};
    Snapshot snapshot;
    const std::optional<std::uint32_t> tick = reader.read_u32();
    const std::optional<std::uint8_t> player_count = reader.read_u8();
    if (!tick || !player_count || *player_count > tuning::max_players) {
        return std::nullopt;
    }
    snapshot.tick = *tick;
    for (std::uint8_t index = 0; index < *player_count; ++index) {
        const std::optional<std::uint8_t> player = reader.read_u8();
        const std::optional<std::uint32_t> ship = reader.read_u32();
        const std::optional<std::uint8_t> lives = reader.read_u8();
        const std::optional<std::uint32_t> score = reader.read_u32();
        if (!player || !ship || !lives || !score || *player >= tuning::max_players) {
            return std::nullopt;
        }
        snapshot.players.push_back(
            PlayerRecord{.player = *player, .ship = *ship, .lives = *lives, .score = *score});
    }
    const std::optional<std::uint16_t> entity_count = reader.read_u16();
    if (!entity_count) {
        return std::nullopt;
    }
    for (std::uint16_t index = 0; index < *entity_count; ++index) {
        const std::optional<std::uint32_t> id = reader.read_u32();
        const std::optional<std::uint8_t> kind = reader.read_u8();
        const std::optional<float> x = reader.read_f32();
        const std::optional<float> y = reader.read_f32();
        const std::optional<float> vx = reader.read_f32();
        const std::optional<float> vy = reader.read_f32();
        const std::optional<std::uint8_t> frame = reader.read_u8();
        if (!id || !kind || !x || !y || !vx || !vy || !frame || !std::isfinite(*x) ||
            !std::isfinite(*y) || !std::isfinite(*vx) || !std::isfinite(*vy)) {
            return std::nullopt;
        }
        snapshot.entities.push_back(EntityRecord{
            .id = *id, .kind = *kind, .x = *x, .y = *y, .vx = *vx, .vy = *vy, .frame = *frame});
    }
    if (!reader.at_end()) {
        return std::nullopt;
    }
    return snapshot;
}

} // namespace rtype
