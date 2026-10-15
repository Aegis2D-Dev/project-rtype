/// @file
/// @brief Snapshots of the world sent to the clients (RFC section 4.3).
#pragma once
#include <aegis2d/net/packet.hpp>

#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtype {

class GameInstance;

/// @brief Kind of an entity in snapshots and events (RFC section 4.1).
/// @details The values are part of the protocol: never renumber them.
enum class EntityKind : std::uint8_t {
    /// @brief A player's ship.
    PlayerShip = 1,
    /// @brief A missile fired by a player.
    PlayerMissile = 2,
    /// @brief A Bydos slave.
    BydosSlave = 3,
    /// @brief A missile fired by an enemy.
    EnemyMissile = 4,
};

/// @brief Network id that names no entity, for a player out of lives.
inline constexpr std::uint32_t no_entity = 0xFFFFFFFF;

/// @brief Size of a player record in a snapshot, in bytes.
inline constexpr std::size_t player_record_size = 10;
/// @brief Size of an entity record in a snapshot, in bytes.
inline constexpr std::size_t entity_record_size = 22;
/// @brief Bytes of a snapshot payload before its records: tick, player and entity counts.
inline constexpr std::size_t snapshot_fixed_size = 4 + 1 + 2;
/// @brief Most entity records a datagram holds next to 4 player records: 51.
inline constexpr std::size_t max_entities_per_datagram =
    (aegis2d::net::PacketHeader::max_payload_size - snapshot_fixed_size -
     (4 * player_record_size)) /
    entity_record_size;

/// @brief What a snapshot says of a player.
struct PlayerRecord {
    /// @brief Player number, 0 to 3.
    std::uint8_t player = 0;
    /// @brief Network id of its ship, or no_entity once out of lives.
    std::uint32_t ship = no_entity;
    /// @brief Lives left.
    std::uint8_t lives = 0;
    /// @brief Score.
    std::uint32_t score = 0;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const PlayerRecord&) const = default;
};

/// @brief What a snapshot says of an entity.
struct EntityRecord {
    /// @brief Network id.
    std::uint32_t id = 0;
    /// @brief Kind, an EntityKind value (others are reserved).
    std::uint8_t kind = 0;
    /// @brief Position of its center, in logical pixels.
    float x = 0.0F;
    /// @copydoc x
    float y = 0.0F;
    /// @brief Velocity, in logical pixels per second.
    float vx = 0.0F;
    /// @copydoc vx
    float vy = 0.0F;
    /// @brief Frame of its sprite sheet to show.
    std::uint8_t frame = 0;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const EntityRecord&) const = default;
};

/// @brief The state of the world at one tick.
struct Snapshot {
    /// @brief Server tick of the state.
    std::uint32_t tick = 0;
    /// @brief Every player of the game.
    std::vector<PlayerRecord> players;
    /// @brief Every replicated entity of the world.
    std::vector<EntityRecord> entities;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const Snapshot&) const = default;
};

/// @brief Tells whether a snapshot is due after a tick.
/// @details tuning::snapshot_rate times per second: every third tick at 60 ticks per second.
/// @param tick Number of ticks run, `ServerGame::ticks()`.
/// @return `true` every tuning::tick_rate / tuning::snapshot_rate ticks.
[[nodiscard]] constexpr bool snapshot_due(std::uint64_t tick) noexcept {
    return tick % (tuning::tick_rate / tuning::snapshot_rate) == 0;
}

/// @brief Takes the state of the game, on the simulation thread.
/// @details Players in their number order; entities with a `NetworkId`, a `Transform` and
/// a `Collider` whose layer gives their kind (entities of another layer are left out).
/// @param instance Game to read; the scores of players out of lives are the last ones seen.
/// @return The snapshot.
[[nodiscard]] Snapshot take_snapshot(GameInstance& instance);

/// @brief Encodes a snapshot as the payloads of `Snapshot` datagrams, header excluded.
/// @details Each payload holds the tick, every player record and up to
/// max_entities_per_datagram entity records, so that it fits a datagram; a snapshot with no
/// entity still gives one payload.
/// @param snapshot State to send.
/// @return The payloads, in order.
[[nodiscard]] std::vector<std::vector<std::byte>> write_snapshot(const Snapshot& snapshot);

/// @brief Decodes the payload of one `Snapshot` datagram.
/// @param payload Bytes after the header.
/// @return Its tick and records, or `std::nullopt` if the payload is malformed (a record
/// past the end, bytes after the last one, more than 4 players, a player above 3, or a
/// position or velocity that is not finite).
[[nodiscard]] std::optional<Snapshot> read_snapshot(std::span<const std::byte> payload);

} // namespace rtype
