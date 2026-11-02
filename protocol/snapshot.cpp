#include <aegis2d/net/bits.hpp>

#include "snapshot_fields.hpp"
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

constexpr unsigned player_count_bits = 3;
constexpr unsigned player_bits = 2;
constexpr unsigned lives_bits = 8;

std::size_t player_record_bits(const PlayerRecord& player) noexcept {
    const bool has_ship = player.ship != no_entity;
    return player_bits + 1 + (has_ship ? varint_bits(player.ship) : 0) + lives_bits +
           varint_bits(player.score);
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

} // namespace

namespace fields {

std::size_t players_bits(const Snapshot& snapshot) noexcept {
    std::size_t bits = 32 + player_count_bits;
    for (const PlayerRecord& player : snapshot.players) {
        bits += player_record_bits(player);
    }
    return bits;
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

std::optional<Snapshot> read_players(net::BitReader& reader) {
    const std::optional<std::uint64_t> tick = reader.read_bits(32);
    const std::optional<std::uint64_t> player_count = reader.read_bits(player_count_bits);
    if (!tick || !player_count || *player_count > tuning::max_players) {
        return std::nullopt;
    }
    Snapshot snapshot{.tick = static_cast<std::uint32_t>(*tick), .players = {}, .entities = {}};
    for (std::uint64_t index = 0; index < *player_count; ++index) {
        const std::optional<PlayerRecord> player = read_player(reader);
        if (!player) {
            return std::nullopt;
        }
        snapshot.players.push_back(*player);
    }
    return snapshot;
}

void write_id(net::BitWriter& writer, std::uint32_t id, std::uint32_t previous) {
    write_varint(writer, id_low(id) - previous);
    writer.write_bits(id >> id_low_bits, id_high_bits);
}

std::optional<std::uint32_t> read_id(net::BitReader& reader, std::uint32_t previous) {
    const std::optional<std::uint32_t> gap = read_varint(reader);
    const std::optional<std::uint64_t> high = reader.read_bits(id_high_bits);
    if (!gap || !high || *gap > id_low_mask - previous) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>((*high << id_low_bits) | (previous + *gap));
}

void write_fields(net::BitWriter& writer, const EntityRecord& entity) {
    writer.write_bits(entity.kind, kind_bits);
    write_position(writer, entity.x);
    write_position(writer, entity.y);
    write_velocity(writer, entity.vx);
    write_velocity(writer, entity.vy);
    writer.write_bits(std::min(entity.frame, max_frame), frame_bits);
}

std::optional<EntityRecord> read_fields(net::BitReader& reader, std::uint32_t id) {
    const std::optional<std::uint64_t> kind = reader.read_bits(kind_bits);
    const std::optional<float> x = read_position(reader);
    const std::optional<float> y = read_position(reader);
    const std::optional<float> vx = read_velocity(reader);
    const std::optional<float> vy = read_velocity(reader);
    const std::optional<std::uint64_t> frame = reader.read_bits(frame_bits);
    if (!kind || !x || !y || !vx || !vy || !frame) {
        return std::nullopt;
    }
    return EntityRecord{.id = id,
                        .kind = static_cast<std::uint8_t>(*kind),
                        .x = *x,
                        .y = *y,
                        .vx = *vx,
                        .vy = *vy,
                        .frame = static_cast<std::uint8_t>(*frame)};
}

} // namespace fields

std::size_t entity_record_bits(std::uint32_t id_gap) noexcept {
    return varint_bits(id_gap) + entity_record_fixed_bits;
}

std::vector<std::vector<std::byte>> write_snapshot(const Snapshot& snapshot) {
    std::vector<EntityRecord> entities = snapshot.entities;
    std::ranges::sort(entities, {},
                      [](const EntityRecord& entity) { return fields::id_order(entity.id); });
    const std::size_t fixed = fields::players_bits(snapshot) + fields::entity_count_bits;

    // Records in order of their ids' low bits; each payload starts its gaps from 0, so it is
    // read alone. At least one payload, so a world with no entity still sends its players.
    std::vector<std::vector<std::byte>> payloads;
    std::size_t first = 0;
    for (bool more = true; more; more = first < entities.size()) {
        std::size_t bits = fixed;
        std::size_t end = first;
        std::uint32_t previous = 0;
        while (end < entities.size()) {
            const std::uint32_t low = fields::id_low(entities.at(end).id);
            const std::size_t size = entity_record_bits(low - previous);
            if (bits + size > fields::payload_bits) {
                break;
            }
            bits += size;
            previous = low;
            ++end;
        }
        net::BitWriter writer;
        fields::write_players(writer, snapshot);
        writer.write_bits(end - first, fields::entity_count_bits);
        previous = 0;
        for (std::size_t index = first; index < end; ++index) {
            const EntityRecord& entity = entities.at(index);
            fields::write_id(writer, entity.id, previous);
            fields::write_fields(writer, entity);
            previous = fields::id_low(entity.id);
        }
        payloads.emplace_back(writer.data().begin(), writer.data().end());
        first = end;
    }
    return payloads;
}

std::optional<Snapshot> read_snapshot(std::span<const std::byte> payload) {
    net::BitReader reader{payload};
    std::optional<Snapshot> snapshot = fields::read_players(reader);
    const std::optional<std::uint64_t> entity_count = reader.read_bits(fields::entity_count_bits);
    if (!snapshot || !entity_count) {
        return std::nullopt;
    }
    std::uint32_t previous = 0;
    for (std::uint64_t index = 0; index < *entity_count; ++index) {
        const std::optional<std::uint32_t> id = fields::read_id(reader, previous);
        const std::optional<EntityRecord> entity =
            id ? fields::read_fields(reader, *id) : std::nullopt;
        if (!entity) {
            return std::nullopt;
        }
        previous = fields::id_low(entity->id);
        snapshot->entities.push_back(*entity);
    }
    if (!reader.at_end()) {
        return std::nullopt;
    }
    return snapshot;
}

} // namespace rtype
