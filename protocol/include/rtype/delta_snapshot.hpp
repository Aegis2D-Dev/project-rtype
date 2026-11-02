/// @file
/// @brief Snapshots encoded against a baseline the client acknowledged: only what changed.
#pragma once
#include <rtype/snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtype {

/// @brief What a delta snapshot says of an entity.
/// @details The values are part of the payload: never renumber them.
enum class EntityChange : std::uint8_t {
    /// @brief Some fields changed since the baseline; EntityDelta::fields says which.
    Updated = 0,
    /// @brief Not in the baseline (or no baseline): every field is sent.
    Added = 1,
    /// @brief In the baseline, gone since.
    Removed = 2,
};

/// @brief Fields of an EntityChange::Updated record, one bit each.
enum class EntityField : std::uint8_t {
    /// @brief `x`.
    X = 1U << 0U,
    /// @brief `y`.
    Y = 1U << 1U,
    /// @brief `vx` and `vy`, together.
    Velocity = 1U << 2U,
    /// @brief `frame`.
    Frame = 1U << 3U,
};

/// @brief Bits of the change of a delta record.
inline constexpr unsigned change_bits = 2;
/// @brief Bits of the fields of an EntityChange::Updated record.
inline constexpr unsigned entity_field_bits = 4;

/// @brief What a delta snapshot says of one entity.
struct EntityDelta {
    /// @brief How it differs from the baseline.
    EntityChange change = EntityChange::Added;
    /// @brief For EntityChange::Updated, the EntityField bits of the fields sent.
    std::uint8_t fields = 0;
    /// @brief Its id, and the fields sent; the others are 0.
    EntityRecord record;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const EntityDelta&) const = default;
};

/// @brief One payload of a snapshot encoded against a baseline.
struct DeltaSnapshot {
    /// @brief Server tick of the state.
    std::uint32_t tick = 0;
    /// @brief Tick of the baseline, or nothing for a full snapshot.
    std::optional<std::uint32_t> baseline;
    /// @brief Every player of the game, as in a full snapshot.
    std::vector<PlayerRecord> players;
    /// @brief The entities added, changed or removed since the baseline.
    std::vector<EntityDelta> entities;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const DeltaSnapshot&) const = default;
};

/// @brief A record as the client gets it: position and velocity rounded as compact.hpp
/// does, frame at most 15.
/// @param entity Record of the server.
/// @return The record after a trip through a payload.
[[nodiscard]] EntityRecord quantized(const EntityRecord& entity) noexcept;

/// @brief Encodes a snapshot against a baseline, as the payloads of `Snapshot` datagrams.
/// @details Entities whose quantized fields are all the same as in the baseline are
/// skipped; changed ones carry only the fields that changed; new ones carry every field;
/// those of the baseline that are gone are listed as removed. Without a baseline, every
/// entity is added: a full snapshot. Records go by increasing index, as in write_snapshot(),
/// and each payload holds the tick, the baseline, every player record and as many records
/// as fit a datagram.
/// @param snapshot State to send.
/// @param baseline A snapshot the client acknowledged, older than @p snapshot, or `nullptr`.
/// @return The payloads, in order; at least one.
/// @throws std::invalid_argument if a field does not fit, as write_snapshot(), or if
/// @p baseline is not older than @p snapshot.
[[nodiscard]] std::vector<std::vector<std::byte>> write_delta_snapshot(const Snapshot& snapshot,
                                                                       const Snapshot* baseline);

/// @brief Decodes one payload written by write_delta_snapshot().
/// @param payload Bytes after the header.
/// @return The payload, or `std::nullopt` if it is malformed (as read_snapshot(), plus a
/// baseline not older than the tick, a change of 3, an update of no field, or a removal
/// without baseline).
[[nodiscard]] std::optional<DeltaSnapshot> read_delta_snapshot(std::span<const std::byte> payload);

/// @brief Rebuilds a snapshot from its baseline and every payload of its tick.
/// @param baseline The snapshot of DeltaSnapshot::baseline, as the client rebuilt it, or
/// `nullptr` for a full snapshot.
/// @param parts Every payload of the tick, in any order.
/// @return The snapshot, its entities in the order of write_snapshot(), or `std::nullopt` if
/// @p parts is empty, mixes ticks or baselines, does not match @p baseline, or updates or
/// removes an entity the baseline does not have.
[[nodiscard]] std::optional<Snapshot> apply_delta(const Snapshot* baseline,
                                                  std::span<const DeltaSnapshot> parts);

} // namespace rtype
