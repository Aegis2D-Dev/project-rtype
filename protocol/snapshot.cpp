#include <aegis2d/net/bits.hpp>
#include <aegis2d/net/packet.hpp>

#include <rtype/compact.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

namespace {

constexpr std::size_t payload_bits = net::PacketHeader::max_payload_size * 8;
constexpr unsigned player_count_bits = 3;
constexpr unsigned player_bits = 2;
constexpr unsigned lives_bits = 8;
constexpr unsigned entity_count_bits = 16;
constexpr std::uint8_t max_frame = (1U << frame_bits) - 1U;
constexpr std::uint32_t id_low_mask = (std::uint32_t{1} << id_low_bits) - 1U;

constexpr std::uint32_t low(std::uint32_t id) noexcept {
    return id & id_low_mask;
}

std::size_t player_record_bits(const PlayerRecord& player) noexcept {
    const bool has_ship = player.ship != no_entity;
    return player_bits + 1 + (has_ship ? varint_bits(player.ship) : 0) + lives_bits +
           varint_bits(player.score);
}

void write_players(net::BitWriter& writer, const Snapshot& snapshot) {
    writer.write_bits(snapshot.tick, 32);
    writer.write_bits(snapshot.players.size(), player_count_bits);
    for (const PlayerRecord& player : snapshot.players) {
        writer.write_bits(player.player, player_bits);
        writer.write_bool(player.ship != no_entity);
        if (player.ship != no_entity) {
            write_varint(writer, player.ship);
        }
        writer.write_bits(player.lives, lives_bits);
        write_varint(writer, player.score);
    }
}

// previous: the low bits of the previous record's id, 0 for the first one.
void write_entity(net::BitWriter& writer, const EntityRecord& entity, std::uint32_t previous) {
    write_varint(writer, low(entity.id) - previous);
    writer.write_bits(entity.id >> id_low_bits, id_high_bits);
    writer.write_bits(entity.kind, kind_bits);
    write_position(writer, entity.x);
    write_position(writer, entity.y);
    write_velocity(writer, entity.vx);
    write_velocity(writer, entity.vy);
    writer.write_bits(std::min(entity.frame, max_frame), frame_bits);
}

std::optional<PlayerRecord> read_player(net::BitReader& reader) {
    const std::optional<std::uint64_t> player = reader.read_bits(player_bits);
    const std::optional<bool> has_ship = reader.read_bool();
    if (!player || !has_ship) {
        return std::nullopt;
    }
    const std::optional<std::uint32_t> ship =
        *has_ship ? read_varint(reader) : std::optional{no_entity};
    const std::optional<std::uint64_t> lives = reader.read_bits(lives_bits);
    const std::optional<std::uint32_t> score = read_varint(reader);
    if (!ship || !lives || !score) {
        return std::nullopt;
    }
    return PlayerRecord{.player = static_cast<std::uint8_t>(*player),
                        .ship = *ship,
                        .lives = static_cast<std::uint8_t>(*lives),
                        .score = *score};
}

std::optional<EntityRecord> read_entity(net::BitReader& reader, std::uint32_t previous) {
    const std::optional<std::uint32_t> gap = read_varint(reader);
    const std::optional<std::uint64_t> high = reader.read_bits(id_high_bits);
    const std::optional<std::uint64_t> kind = reader.read_bits(kind_bits);
    const std::optional<float> x = read_position(reader);
    const std::optional<float> y = read_position(reader);
    const std::optional<float> vx = read_velocity(reader);
    const std::optional<float> vy = read_velocity(reader);
    const std::optional<std::uint64_t> frame = reader.read_bits(frame_bits);
    if (!gap || !high || !kind || !x || !y || !vx || !vy || !frame ||
        *gap > id_low_mask - previous) {
        return std::nullopt;
    }
    const auto id = static_cast<std::uint32_t>((*high << id_low_bits) | (previous + *gap));
    return EntityRecord{.id = id,
                        .kind = static_cast<std::uint8_t>(*kind),
                        .x = *x,
                        .y = *y,
                        .vx = *vx,
                        .vy = *vy,
                        .frame = static_cast<std::uint8_t>(*frame)};
}

} // namespace

std::size_t entity_record_bits(std::uint32_t id_gap) noexcept {
    return varint_bits(id_gap) + entity_record_fixed_bits;
}

std::vector<std::vector<std::byte>> write_snapshot(const Snapshot& snapshot) {
    std::vector<EntityRecord> entities = snapshot.entities;
    std::ranges::sort(entities, {}, [](const EntityRecord& entity) {
        return (std::uint64_t{low(entity.id)} << 32U) | entity.id;
    });
    std::size_t fixed = snapshot_fixed_bits;
    for (const PlayerRecord& player : snapshot.players) {
        fixed += player_record_bits(player);
    }

    // Records in order of their ids' low bits; each payload starts its gaps from 0, so it is
    // read alone. At least one payload, so a world with no entity still sends its players.
    std::vector<std::vector<std::byte>> payloads;
    std::size_t first = 0;
    for (bool more = true; more; more = first < entities.size()) {
        std::size_t bits = fixed;
        std::size_t end = first;
        std::uint32_t previous = 0;
        while (end < entities.size() &&
               bits + entity_record_bits(low(entities.at(end).id) - previous) <= payload_bits) {
            bits += entity_record_bits(low(entities.at(end).id) - previous);
            previous = low(entities.at(end).id);
            ++end;
        }
        net::BitWriter writer;
        write_players(writer, snapshot);
        writer.write_bits(end - first, entity_count_bits);
        previous = 0;
        for (std::size_t index = first; index < end; ++index) {
            write_entity(writer, entities.at(index), previous);
            previous = low(entities.at(index).id);
        }
        payloads.emplace_back(writer.data().begin(), writer.data().end());
        first = end;
    }
    return payloads;
}

std::optional<Snapshot> read_snapshot(std::span<const std::byte> payload) {
    net::BitReader reader{payload};
    Snapshot snapshot;
    const std::optional<std::uint64_t> tick = reader.read_bits(32);
    const std::optional<std::uint64_t> player_count = reader.read_bits(player_count_bits);
    if (!tick || !player_count || *player_count > tuning::max_players) {
        return std::nullopt;
    }
    snapshot.tick = static_cast<std::uint32_t>(*tick);
    for (std::uint64_t index = 0; index < *player_count; ++index) {
        const std::optional<PlayerRecord> player = read_player(reader);
        if (!player) {
            return std::nullopt;
        }
        snapshot.players.push_back(*player);
    }
    const std::optional<std::uint64_t> entity_count = reader.read_bits(entity_count_bits);
    if (!entity_count) {
        return std::nullopt;
    }
    std::uint32_t previous = 0;
    for (std::uint64_t index = 0; index < *entity_count; ++index) {
        const std::optional<EntityRecord> entity = read_entity(reader, previous);
        if (!entity) {
            return std::nullopt;
        }
        previous = low(entity->id);
        snapshot.entities.push_back(*entity);
    }
    if (!reader.at_end()) {
        return std::nullopt;
    }
    return snapshot;
}

} // namespace rtype
