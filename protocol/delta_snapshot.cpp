#include <aegis2d/net/bits.hpp>

#include "snapshot_fields.hpp"
#include <rtype/compact.hpp>
#include <rtype/delta_snapshot.hpp>
#include <rtype/snapshot.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

namespace {

EntityRecord only_id(std::uint32_t id) noexcept {
    EntityRecord entity;
    entity.id = id;
    return entity;
}

constexpr auto bit(EntityField field) noexcept {
    return static_cast<std::uint8_t>(field);
}

bool has(std::uint8_t fields, EntityField field) noexcept {
    return (fields & bit(field)) != 0U;
}

// The fields of two quantized records that differ.
std::uint8_t changed_fields(const EntityRecord& now, const EntityRecord& before) noexcept {
    std::uint8_t fields = 0;
    fields |= now.x != before.x ? bit(EntityField::X) : 0U;
    fields |= now.y != before.y ? bit(EntityField::Y) : 0U;
    fields |= now.vx != before.vx || now.vy != before.vy ? bit(EntityField::Velocity) : 0U;
    fields |= now.frame != before.frame ? bit(EntityField::Frame) : 0U;
    return fields;
}

// Bits of a record after its id.
std::size_t delta_bits(const EntityDelta& delta) noexcept {
    switch (delta.change) {
    case EntityChange::Updated:
        return change_bits + entity_field_bits +
               (has(delta.fields, EntityField::X) ? position_bits : 0U) +
               (has(delta.fields, EntityField::Y) ? position_bits : 0U) +
               (has(delta.fields, EntityField::Velocity) ? 2U * velocity_bits : 0U) +
               (has(delta.fields, EntityField::Frame) ? frame_bits : 0U);
    case EntityChange::Added:
        return change_bits + entity_record_fixed_bits - id_high_bits;
    case EntityChange::Removed:
        break;
    }
    return change_bits;
}

void write_delta(net::BitWriter& writer, const EntityDelta& delta) {
    writer.write_bits(static_cast<std::uint8_t>(delta.change), change_bits);
    const EntityRecord& entity = delta.record;
    switch (delta.change) {
    case EntityChange::Updated:
        writer.write_bits(delta.fields, entity_field_bits);
        if (has(delta.fields, EntityField::X)) {
            write_position(writer, entity.x);
        }
        if (has(delta.fields, EntityField::Y)) {
            write_position(writer, entity.y);
        }
        if (has(delta.fields, EntityField::Velocity)) {
            write_velocity(writer, entity.vx);
            write_velocity(writer, entity.vy);
        }
        if (has(delta.fields, EntityField::Frame)) {
            writer.write_bits(entity.frame, frame_bits);
        }
        break;
    case EntityChange::Added:
        fields::write_fields(writer, entity);
        break;
    case EntityChange::Removed:
        break;
    }
}

std::optional<EntityRecord> read_updated(net::BitReader& reader, std::uint32_t id,
                                         std::uint8_t fields) {
    EntityRecord entity = only_id(id);
    std::optional<float> x = 0.0F;
    std::optional<float> y = 0.0F;
    std::optional<float> vx = 0.0F;
    std::optional<float> vy = 0.0F;
    std::optional<std::uint64_t> frame = 0;
    if (has(fields, EntityField::X)) {
        x = read_position(reader);
    }
    if (has(fields, EntityField::Y)) {
        y = read_position(reader);
    }
    if (has(fields, EntityField::Velocity)) {
        vx = read_velocity(reader);
        vy = read_velocity(reader);
    }
    if (has(fields, EntityField::Frame)) {
        frame = reader.read_bits(frame_bits);
    }
    if (!x || !y || !vx || !vy || !frame) {
        return std::nullopt;
    }
    entity.x = *x;
    entity.y = *y;
    entity.vx = *vx;
    entity.vy = *vy;
    entity.frame = static_cast<std::uint8_t>(*frame);
    return entity;
}

std::optional<EntityDelta> read_delta(net::BitReader& reader, std::uint32_t id, bool has_baseline) {
    const std::optional<std::uint64_t> change = reader.read_bits(change_bits);
    if (!change) {
        return std::nullopt;
    }
    switch (static_cast<EntityChange>(*change)) {
    case EntityChange::Updated: {
        const std::optional<std::uint64_t> fields = reader.read_bits(entity_field_bits);
        if (!has_baseline || !fields || *fields == 0U) {
            return std::nullopt;
        }
        const auto mask = static_cast<std::uint8_t>(*fields);
        const std::optional<EntityRecord> entity = read_updated(reader, id, mask);
        if (!entity) {
            return std::nullopt;
        }
        return EntityDelta{.change = EntityChange::Updated, .fields = mask, .record = *entity};
    }
    case EntityChange::Added: {
        const std::optional<EntityRecord> entity = fields::read_fields(reader, id);
        if (!entity) {
            return std::nullopt;
        }
        return EntityDelta{.change = EntityChange::Added, .fields = 0, .record = *entity};
    }
    case EntityChange::Removed:
        if (!has_baseline) {
            return std::nullopt;
        }
        return EntityDelta{.change = EntityChange::Removed, .fields = 0, .record = only_id(id)};
    }
    return std::nullopt; // change 3 is reserved
}

// The entities of a snapshot by id, quantized.
std::unordered_map<std::uint32_t, EntityRecord> by_id(const Snapshot& snapshot) {
    std::unordered_map<std::uint32_t, EntityRecord> entities;
    for (const EntityRecord& entity : snapshot.entities) {
        entities.insert_or_assign(entity.id, quantized(entity));
    }
    return entities;
}

std::vector<EntityDelta> diff(const Snapshot& snapshot, const Snapshot* baseline) {
    std::unordered_map<std::uint32_t, EntityRecord> before;
    if (baseline != nullptr) {
        before = by_id(*baseline);
    }
    std::vector<EntityDelta> deltas;
    for (const EntityRecord& raw : snapshot.entities) {
        const EntityRecord entity = quantized(raw);
        const auto found = before.find(entity.id);
        if (found == before.end() || found->second.kind != entity.kind) {
            deltas.push_back(
                EntityDelta{.change = EntityChange::Added, .fields = 0, .record = entity});
        } else if (const std::uint8_t fields = changed_fields(entity, found->second);
                   fields != 0U) {
            deltas.push_back(
                EntityDelta{.change = EntityChange::Updated, .fields = fields, .record = entity});
        }
        if (found != before.end()) {
            before.erase(found);
        }
    }
    for (const auto& [id, entity] : before) {
        deltas.push_back(
            EntityDelta{.change = EntityChange::Removed, .fields = 0, .record = only_id(id)});
    }
    std::ranges::sort(deltas, {},
                      [](const EntityDelta& delta) { return fields::id_order(delta.record.id); });
    return deltas;
}

} // namespace

EntityRecord quantized(const EntityRecord& entity) noexcept {
    return EntityRecord{.id = entity.id,
                        .kind = entity.kind,
                        .x = dequantize_position(quantize_position(entity.x)),
                        .y = dequantize_position(quantize_position(entity.y)),
                        .vx = static_cast<float>(quantize_velocity(entity.vx)),
                        .vy = static_cast<float>(quantize_velocity(entity.vy)),
                        .frame = std::min(entity.frame, fields::max_frame)};
}

std::vector<std::vector<std::byte>> write_delta_snapshot(const Snapshot& snapshot,
                                                         const Snapshot* baseline) {
    if (baseline != nullptr && baseline->tick >= snapshot.tick) {
        throw std::invalid_argument{"write_delta_snapshot: the baseline is not older"};
    }
    const std::vector<EntityDelta> deltas = diff(snapshot, baseline);
    const std::uint32_t age = baseline != nullptr ? snapshot.tick - baseline->tick : 0;
    const std::size_t fixed = fields::players_bits(snapshot) + 1 +
                              (baseline != nullptr ? varint_bits(age) : 0) +
                              fields::entity_count_bits;

    // As write_snapshot(): each payload starts its gaps from 0, and there is at least one.
    std::vector<std::vector<std::byte>> payloads;
    std::size_t first = 0;
    for (bool more = true; more; more = first < deltas.size()) {
        std::size_t bits = fixed;
        std::size_t end = first;
        std::uint32_t previous = 0;
        while (end < deltas.size()) {
            const std::uint32_t low = fields::id_low(deltas.at(end).record.id);
            const std::size_t size =
                varint_bits(low - previous) + id_high_bits + delta_bits(deltas.at(end));
            if (bits + size > fields::payload_bits) {
                break;
            }
            bits += size;
            previous = low;
            ++end;
        }
        net::BitWriter writer;
        fields::write_players(writer, snapshot);
        writer.write_bool(baseline != nullptr);
        if (baseline != nullptr) {
            write_varint(writer, age);
        }
        writer.write_bits(end - first, fields::entity_count_bits);
        previous = 0;
        for (std::size_t index = first; index < end; ++index) {
            const EntityDelta& delta = deltas.at(index);
            fields::write_id(writer, delta.record.id, previous);
            write_delta(writer, delta);
            previous = fields::id_low(delta.record.id);
        }
        payloads.emplace_back(writer.data().begin(), writer.data().end());
        first = end;
    }
    return payloads;
}

std::optional<DeltaSnapshot> read_delta_snapshot(std::span<const std::byte> payload) {
    net::BitReader reader{payload};
    const std::optional<Snapshot> players = fields::read_players(reader);
    const std::optional<bool> has_baseline = reader.read_bool();
    if (!players || !has_baseline) {
        return std::nullopt;
    }
    DeltaSnapshot delta{
        .tick = players->tick, .baseline = {}, .players = players->players, .entities = {}};
    if (*has_baseline) {
        const std::optional<std::uint32_t> age = read_varint(reader);
        if (!age || *age == 0U || *age > delta.tick) {
            return std::nullopt;
        }
        delta.baseline = delta.tick - *age;
    }
    const std::optional<std::uint64_t> count = reader.read_bits(fields::entity_count_bits);
    if (!count) {
        return std::nullopt;
    }
    std::uint32_t previous = 0;
    for (std::uint64_t index = 0; index < *count; ++index) {
        const std::optional<std::uint32_t> id = fields::read_id(reader, previous);
        const std::optional<EntityDelta> entity =
            id ? read_delta(reader, *id, *has_baseline) : std::nullopt;
        if (!entity) {
            return std::nullopt;
        }
        previous = fields::id_low(entity->record.id);
        delta.entities.push_back(*entity);
    }
    if (!reader.at_end()) {
        return std::nullopt;
    }
    return delta;
}

std::optional<Snapshot> apply_delta(const Snapshot* baseline,
                                    std::span<const DeltaSnapshot> parts) {
    if (parts.empty()) {
        return std::nullopt;
    }
    const DeltaSnapshot& head = parts.front();
    const bool matches = head.baseline.has_value()
                             ? baseline != nullptr && baseline->tick == head.baseline.value_or(0)
                             : baseline == nullptr;
    if (!matches) {
        return std::nullopt;
    }
    std::unordered_map<std::uint32_t, EntityRecord> entities;
    if (baseline != nullptr) {
        entities = by_id(*baseline);
    }
    for (const DeltaSnapshot& part : parts) {
        if (part.tick != head.tick || part.baseline != head.baseline) {
            return std::nullopt;
        }
        for (const EntityDelta& delta : part.entities) {
            const EntityRecord& record = delta.record;
            const auto found = entities.find(record.id);
            if (delta.change == EntityChange::Added) {
                entities.insert_or_assign(record.id, record);
                continue;
            }
            if (found == entities.end()) {
                return std::nullopt;
            }
            if (delta.change == EntityChange::Removed) {
                entities.erase(found);
                continue;
            }
            EntityRecord& entity = found->second;
            if (has(delta.fields, EntityField::X)) {
                entity.x = record.x;
            }
            if (has(delta.fields, EntityField::Y)) {
                entity.y = record.y;
            }
            if (has(delta.fields, EntityField::Velocity)) {
                entity.vx = record.vx;
                entity.vy = record.vy;
            }
            if (has(delta.fields, EntityField::Frame)) {
                entity.frame = record.frame;
            }
        }
    }
    Snapshot snapshot{.tick = head.tick, .players = head.players, .entities = {}};
    for (const auto& [id, entity] : entities) {
        snapshot.entities.push_back(entity);
    }
    std::ranges::sort(snapshot.entities, {},
                      [](const EntityRecord& entity) { return fields::id_order(entity.id); });
    return snapshot;
}

} // namespace rtype
