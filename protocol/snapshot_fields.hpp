// The fields shared by full and delta snapshot payloads (RFC section 4.3), for snapshot.cpp
// and delta_snapshot.cpp only.
#pragma once
#include <aegis2d/net/bits.hpp>
#include <aegis2d/net/packet.hpp>

#include <rtype/snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rtype::fields {

/// Bits of a payload.
inline constexpr std::size_t payload_bits = aegis2d::net::PacketHeader::max_payload_size * 8;
/// Bits of the entity count.
inline constexpr unsigned entity_count_bits = 16;
/// Largest frame sent.
inline constexpr std::uint8_t max_frame = (1U << frame_bits) - 1U;
/// Mask of the low bits of an id.
inline constexpr std::uint32_t id_low_mask = (std::uint32_t{1} << id_low_bits) - 1U;

/// The low bits of an id, the index of the engine's Entity handles.
constexpr std::uint32_t id_low(std::uint32_t id) noexcept {
    return id & id_low_mask;
}
/// The order of records in a payload: by the low bits of their ids, then by id.
constexpr std::uint64_t id_order(std::uint32_t id) noexcept {
    return (std::uint64_t{id_low(id)} << 32U) | id;
}

/// Bits of the tick and the player records.
std::size_t players_bits(const Snapshot& snapshot) noexcept;
/// Writes the tick and the player records.
void write_players(aegis2d::net::BitWriter& writer, const Snapshot& snapshot);
/// Reads the tick and the player records into a snapshot without entities.
std::optional<Snapshot> read_players(aegis2d::net::BitReader& reader);

/// Writes an id after the record whose id has the low bits @p previous (0 for the first).
void write_id(aegis2d::net::BitWriter& writer, std::uint32_t id, std::uint32_t previous);
/// Reads an id written by write_id().
std::optional<std::uint32_t> read_id(aegis2d::net::BitReader& reader, std::uint32_t previous);

/// Writes the kind, position, velocity and frame of a record.
void write_fields(aegis2d::net::BitWriter& writer, const EntityRecord& entity);
/// Reads the fields written by write_fields() into a record with the id @p id.
std::optional<EntityRecord> read_fields(aegis2d::net::BitReader& reader, std::uint32_t id);

} // namespace rtype::fields
