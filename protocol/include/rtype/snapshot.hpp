/// @file
/// @brief Snapshots of the world sent to the clients (RFC section 4.3).
#pragma once
#include <aegis2d/net/packet.hpp>

#include <rtype/compact.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtype {

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

/// @brief Bits of the tick, the player count and the entity count of a snapshot payload.
inline constexpr std::size_t snapshot_fixed_bits = 32 + 3 + 16;
/// @brief Low bits of a network id, sent as their gap to those of the previous record: the
/// index of the engine's Entity handles, small and dense among live entities.
inline constexpr unsigned id_low_bits = 24;
/// @brief High bits of a network id, sent as they are: the generation of Entity handles.
inline constexpr unsigned id_high_bits = 8;
/// @brief Bits of the kind of an entity record: kinds 0 to 7.
inline constexpr unsigned kind_bits = 3;
/// @brief Bits of the frame of an entity record: frames above 15 are sent as 15.
inline constexpr unsigned frame_bits = 4;
/// @brief Bits of an entity record besides the gap of its id's low bits: the id's high bits,
/// kind, position, velocity and frame (69).
inline constexpr std::size_t entity_record_fixed_bits =
    id_high_bits + kind_bits + (2 * position_bits) + (2 * velocity_bits) + frame_bits;
/// @brief Largest gap between the low bits of two records' ids that keeps a record within
/// 10 bytes (77 bits).
inline constexpr std::uint32_t short_id_gap = (std::uint32_t{1} << varint_group_bits) - 1U;

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
    /// @brief Sequence of the last `Input` of the player the server applied (InputRecord), or
    /// nothing before the first.
    std::optional<std::uint16_t> input;

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

/// @brief Size of an entity record.
/// @param id_gap The low id_low_bits bits of its id minus those of the record before it in
/// the payload, or those bits themselves for the first record.
/// @return entity_record_fixed_bits plus the bits of @p id_gap as a varint: 77 bits up to
/// short_id_gap.
[[nodiscard]] std::size_t entity_record_bits(std::uint32_t id_gap) noexcept;

/// @brief Encodes a snapshot as the payloads of `Snapshot` datagrams, header excluded.
/// @details Fields are packed bit by bit (RFC section 4.3): positions to the 1/8 px and
/// velocities to the pixel per second, see compact.hpp, and the entities in increasing order
/// of the low bits of their id (then of the id), each id written as the gap of its low bits
/// to the previous record's and its high bits. Each payload holds the tick, every
/// player record and as many entity records as fit a datagram; a snapshot with no entity
/// still gives one payload.
/// @param snapshot State to send: at most 7 player records, player numbers below 4 and
/// entity kinds below 8.
/// @return The payloads, in order.
/// @throws std::invalid_argument if a player count, player number or entity kind does not
/// fit its field.
[[nodiscard]] std::vector<std::vector<std::byte>> write_snapshot(const Snapshot& snapshot);

/// @brief Decodes the payload of one `Snapshot` datagram.
/// @param payload Bytes after the header.
/// @return Its tick and records, the entities in the order they were written, or
/// `std::nullopt` if the payload is malformed (a record past the end, bytes or bits other than
/// the padding after the last one, more than 4 players, a varint with another encoding than
/// write_varint()'s, or the low bits of an id beyond id_low_bits).
[[nodiscard]] std::optional<Snapshot> read_snapshot(std::span<const std::byte> payload);

} // namespace rtype
