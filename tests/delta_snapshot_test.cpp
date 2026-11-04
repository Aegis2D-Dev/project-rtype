#include <aegis2d/core/clock.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/bits.hpp>
#include <aegis2d/net/packet.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/compact.hpp>
#include <rtype/delta_snapshot.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>
#include <rtype/waves.hpp>
#include <rtype/world_snapshot.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

using rtype::DeltaSnapshot;
using rtype::EntityChange;
using rtype::EntityDelta;
using rtype::EntityField;
using rtype::EntityRecord;
using rtype::PlayerRecord;
using rtype::Snapshot;
namespace tuning = rtype::tuning;

namespace {

constexpr auto bits(EntityField field) {
    return static_cast<std::uint8_t>(field);
}

std::vector<std::byte> bytes(std::initializer_list<std::uint8_t> values) {
    std::vector<std::byte> out;
    for (const std::uint8_t value : values) {
        out.push_back(std::byte{value});
    }
    return out;
}

EntityRecord bydos(std::uint32_t id, float x, float y) {
    return EntityRecord{.id = id, .kind = 3, .x = x, .y = y, .vx = -120.0F, .vy = 0.0F, .frame = 1};
}

Snapshot baseline() {
    return Snapshot{
        .tick = 30,
        .players = {PlayerRecord{.player = 0, .ship = 1, .lives = 3, .score = 0, .input = 12}},
        .entities =
            {EntityRecord{
                 .id = 1, .kind = 1, .x = 100.0F, .y = 360.0F, .vx = 0.0F, .vy = 0.0F, .frame = 0},
             bydos(2, 900.0F, 200.0F), bydos(3, 1000.0F, 400.0F)},
    };
}

// Encodes against the baseline, reads each payload back and rebuilds the snapshot.
struct Sent {
    std::vector<std::vector<std::byte>> payloads;
    std::vector<DeltaSnapshot> parts;
    std::optional<Snapshot> rebuilt;
};

Sent send(const Snapshot& snapshot, const Snapshot* base) {
    Sent sent{.payloads = rtype::write_delta_snapshot(snapshot, base), .parts = {}, .rebuilt = {}};
    for (const std::vector<std::byte>& payload : sent.payloads) {
        CHECK(payload.size() <= aegis2d::net::PacketHeader::max_payload_size);
        const std::optional<DeltaSnapshot> part = rtype::read_delta_snapshot(payload);
        REQUIRE(part.has_value());
        sent.parts.push_back(part.value_or(DeltaSnapshot{}));
    }
    sent.rebuilt = rtype::apply_delta(base, sent.parts);
    return sent;
}

// The snapshot as the client must rebuild it: quantized, in the order of the payloads.
Snapshot expected(const Snapshot& snapshot) {
    Snapshot result{.tick = snapshot.tick, .players = snapshot.players, .entities = {}};
    for (const EntityRecord& entity : snapshot.entities) {
        result.entities.push_back(rtype::quantized(entity));
    }
    std::ranges::sort(result.entities, {}, [](const EntityRecord& entity) {
        return ((entity.id & 0xFFFFFFULL) << 32U) | entity.id;
    });
    return result;
}

} // namespace

TEST_CASE("delta snapshots follow the RFC examples bit by bit", "[snapshot][delta]") {
    // RFC section 4.7: player 0 (ship 2, 3 lives, score 100, input 41), its ship alone.
    const Snapshot full{
        .tick = 600,
        .players = {PlayerRecord{.player = 0, .ship = 2, .lives = 3, .score = 100, .input = 41}},
        .entities = {EntityRecord{
            .id = 2, .kind = 1, .x = 100.0F, .y = 360.0F, .vx = 0.0F, .vy = 0.0F, .frame = 0}}};
    const std::vector<std::vector<std::byte>> first = rtype::write_delta_snapshot(full, nullptr);
    REQUIRE(first.size() == 1);
    // 175 bits.
    CHECK(first.front() ==
          bytes({0x58, 0x02, 0x00, 0x00, 0xA1, 0xC0, 0x00, 0xD9, 0x14, 0x00, 0x01,
                 0x00, 0x02, 0x00, 0x05, 0x64, 0x00, 0xB4, 0x00, 0x00, 0x00, 0x00}));
    CHECK(rtype::apply_delta(
              nullptr,
              std::vector{rtype::read_delta_snapshot(first.front()).value_or(DeltaSnapshot{})}) ==
          std::optional{full});

    // Section 4.8: at tick 603, against it, input 44; the ship moved, a Bydos appeared.
    Snapshot next = full;
    next.tick = 603;
    next.players.front().input = 44;
    next.entities.front().x = 105.0F;
    next.entities.front().vx = 300.0F;
    next.entities.push_back(EntityRecord{
        .id = 5, .kind = 3, .x = 1300.0F, .y = 200.0F, .vx = -120.0F, .vy = 0.0F, .frame = 1});
    const std::vector<std::vector<std::byte>> second = rtype::write_delta_snapshot(next, &full);
    REQUIRE(second.size() == 1);
    // 244 bits.
    CHECK(second.front() == bytes({0x5B, 0x02, 0x00, 0x00, 0xA1, 0xC0, 0x00, 0x59, 0x16, 0x80, 0x03,
                                   0x02, 0x00, 0x02, 0x00, 0x14, 0xD2, 0x80, 0x25, 0x00, 0x60, 0x00,
                                   0xA0, 0x81, 0xA2, 0x80, 0x0C, 0x88, 0x0F, 0x00, 0x01}));
    CHECK(rtype::apply_delta(
              &full,
              std::vector{rtype::read_delta_snapshot(second.front()).value_or(DeltaSnapshot{})}) ==
          std::optional{next});
}

TEST_CASE("without a baseline every entity is added", "[snapshot][delta]") {
    const Sent sent = send(baseline(), nullptr);
    REQUIRE(sent.parts.size() == 1);
    CHECK(!sent.parts.front().baseline);
    REQUIRE(sent.parts.front().entities.size() == 3);
    for (const EntityDelta& delta : sent.parts.front().entities) {
        CHECK(delta.change == EntityChange::Added);
    }
    CHECK(sent.rebuilt == expected(baseline()));
}

TEST_CASE("unchanged entities are skipped", "[snapshot][delta]") {
    const Snapshot base = baseline();
    Snapshot same = base;
    same.tick = 33;
    // Less than 1/16 px or 0.5 px/s away: the same once quantized.
    same.entities.at(0).x += 0.01F;
    same.entities.at(1).vx += 0.2F;
    const Sent sent = send(same, &base);
    REQUIRE(sent.parts.size() == 1);
    CHECK(sent.parts.front().baseline == 30U);
    CHECK(sent.parts.front().entities.empty());
    Snapshot rebuilt = expected(base);
    rebuilt.tick = 33;
    CHECK(sent.rebuilt == rebuilt);
    // The tick, the player and its input, the baseline and an empty list: 104 bits, against
    // 326 in full.
    CHECK(sent.payloads.front().size() == 13);
    CHECK(rtype::write_snapshot(same).front().size() == 41);
}

TEST_CASE("moved entities carry only the fields that changed", "[snapshot][delta]") {
    const Snapshot base = baseline();
    Snapshot moved = base;
    moved.tick = 33;
    moved.entities.at(1).x -= 6.0F;                 // a Bydos going left
    moved.entities.at(2).y += 0.5F;                 // a Bydos weaving
    moved.entities.at(2).frame = 0;                 // and moving its legs
    moved.entities.at(0).vx = tuning::player_speed; // the ship starting to the right
    const Sent sent = send(moved, &base);
    REQUIRE(sent.parts.size() == 1);
    const std::vector<EntityDelta>& deltas = sent.parts.front().entities;
    REQUIRE(deltas.size() == 3);
    CHECK(deltas.at(0).change == EntityChange::Updated);
    CHECK(deltas.at(0).fields == bits(EntityField::Velocity));
    CHECK(deltas.at(1).fields == bits(EntityField::X));
    CHECK(deltas.at(1).record.x == 894.0F);
    CHECK(deltas.at(2).fields == (bits(EntityField::Y) | bits(EntityField::Frame)));
    CHECK(sent.rebuilt == expected(moved));
}

TEST_CASE("new and removed entities", "[snapshot][delta]") {
    const Snapshot base = baseline();
    Snapshot next = base;
    next.tick = 36;
    next.entities.erase(next.entities.begin() + 1); // Bydos 2 killed
    next.entities.push_back(EntityRecord{.id = 4,
                                         .kind = 2,
                                         .x = 120.0F,
                                         .y = 360.0F,
                                         .vx = tuning::player_missile_speed,
                                         .vy = 0.0F,
                                         .frame = 0}); // a missile fired
    // Index 2 reused by a later generation: removed, then added.
    next.entities.push_back(bydos(0x01000002U, 1296.0F, 300.0F));
    const Sent sent = send(next, &base);
    REQUIRE(sent.parts.size() == 1);
    const std::vector<EntityDelta>& deltas = sent.parts.front().entities;
    REQUIRE(deltas.size() == 3);
    CHECK(deltas.at(0) ==
          EntityDelta{
              .change = EntityChange::Removed,
              .fields = 0,
              .record = EntityRecord{
                  .id = 2, .kind = 0, .x = 0.0F, .y = 0.0F, .vx = 0.0F, .vy = 0.0F, .frame = 0}});
    CHECK(deltas.at(1).change == EntityChange::Added);
    CHECK(deltas.at(1).record.id == 0x01000002U);
    CHECK(deltas.at(2).change == EntityChange::Added);
    CHECK(deltas.at(2).record == rtype::quantized(next.entities.at(2)));
    CHECK(sent.rebuilt == expected(next));
}

TEST_CASE("a large delta is split into datagrams rebuilt together", "[snapshot][delta]") {
    Snapshot base{.tick = 3, .players = {}, .entities = {}};
    for (std::uint32_t id = 0; id < 400; ++id) {
        base.entities.push_back(bydos(id, static_cast<float>(id), 100.0F));
    }
    Snapshot next = base;
    next.tick = 6;
    for (EntityRecord& entity : next.entities) {
        entity.x += 6.0F;
    }
    const Sent sent = send(next, &base);
    // 400 moved records of 37 bits (index gap, generation, change, fields, x): 2 datagrams.
    CHECK(sent.parts.size() == 2);
    CHECK(sent.rebuilt == expected(next));

    const Sent full = send(next, nullptr);
    CHECK(full.parts.size() == 4);
    CHECK(full.rebuilt == expected(next));
    // Parts of two ticks are refused. A missing part cannot be seen here: the snapshot has
    // the entities of the others, and is not acknowledged, so never a baseline.
    const Snapshot partial =
        rtype::apply_delta(nullptr, std::span{full.parts}.first(3)).value_or(Snapshot{});
    CHECK(!partial.entities.empty());
    CHECK(partial.entities.size() < 400);
    std::vector<DeltaSnapshot> mixed = full.parts;
    mixed.back().tick = 9;
    CHECK(!rtype::apply_delta(nullptr, mixed));
}

TEST_CASE("a delta is rebuilt only from its own baseline", "[snapshot][delta]") {
    const Snapshot base = baseline();
    Snapshot next = base;
    next.tick = 33;
    next.entities.at(1).x -= 6.0F;
    const Sent sent = send(next, &base);
    CHECK(!rtype::apply_delta(nullptr, sent.parts));
    Snapshot other = base;
    other.tick = 27;
    CHECK(!rtype::apply_delta(&other, sent.parts));
    // An update of an entity the baseline does not have.
    Snapshot without = base;
    without.entities.erase(without.entities.begin() + 1);
    CHECK(!rtype::apply_delta(&without, sent.parts));
    CHECK(!rtype::apply_delta(&base, {}));

    CHECK_THROWS_AS(rtype::write_delta_snapshot(base, &next), std::invalid_argument);
    CHECK_THROWS_AS(rtype::write_delta_snapshot(base, &base), std::invalid_argument);
}

TEST_CASE("malformed delta snapshots are rejected", "[snapshot][delta]") {
    const Snapshot base = baseline();
    Snapshot next = base;
    next.tick = 33;
    next.entities.at(1).x -= 6.0F;
    const std::vector<std::byte> valid = rtype::write_delta_snapshot(next, &base).front();
    for (std::size_t size = 0; size < valid.size(); ++size) {
        CHECK(!rtype::read_delta_snapshot(std::span{valid}.first(size)));
    }
    std::vector<std::byte> trailing = valid;
    trailing.push_back(std::byte{0});
    CHECK(!rtype::read_delta_snapshot(trailing));

    // A payload of tick 33, no player, with or without baseline, and one record of index 2.
    const auto payload = [](bool with_baseline, std::uint32_t age, std::uint64_t change,
                            std::uint64_t fields) {
        aegis2d::net::BitWriter writer;
        writer.write_bits(33, 32);
        writer.write_bits(0, 3);
        writer.write_bool(with_baseline);
        if (with_baseline) {
            rtype::write_varint(writer, age);
        }
        writer.write_bits(1, 16);
        rtype::write_varint(writer, 2);
        writer.write_bits(0, rtype::id_high_bits);
        writer.write_bits(change, rtype::change_bits);
        if (change == 0) {
            writer.write_bits(fields, rtype::entity_field_bits);
            writer.write_bits(0, rtype::frame_bits);
        }
        return std::vector<std::byte>{writer.data().begin(), writer.data().end()};
    };
    CHECK(rtype::read_delta_snapshot(payload(true, 3, 0, bits(EntityField::Frame))));
    CHECK(rtype::read_delta_snapshot(payload(true, 3, 2, 0)));
    CHECK(!rtype::read_delta_snapshot(payload(true, 3, 3, 0)));  // reserved change
    CHECK(!rtype::read_delta_snapshot(payload(true, 3, 0, 0)));  // no field
    CHECK(!rtype::read_delta_snapshot(payload(true, 0, 2, 0)));  // baseline of the tick
    CHECK(!rtype::read_delta_snapshot(payload(true, 34, 2, 0))); // before tick 0
    CHECK(!rtype::read_delta_snapshot(payload(false, 0, 2, 0))); // removal, no baseline
    CHECK(!rtype::read_delta_snapshot(payload(false, 0, 0, bits(EntityField::Frame))));
}

TEST_CASE("deltas against acknowledged snapshots rebuild a game exactly", "[snapshot][delta]") {
    rtype::GameInstance instance{7, rtype::load_waves(RTYPE_ASSET_DIR "/waves/stage1.json")};
    for (std::uint8_t player = 0; player < tuning::max_players; ++player) {
        instance.join(player);
        aegis2d::ecs::PlayerInput input;
        input.set(aegis2d::ecs::Action::Fire, true);
        input.set(player % 2 == 0 ? aegis2d::ecs::Action::Down : aegis2d::ecs::Action::Right, true);
        instance.set_input(player, input, player);
    }
    // The client acknowledges every other snapshot: the baseline is 2 snapshots old.
    std::vector<Snapshot> sent;
    std::size_t delta_bytes = 0;
    std::size_t full_bytes = 0;
    for (std::uint64_t tick = 1; tick <= 20ULL * tuning::tick_rate; ++tick) {
        instance.game().tick(aegis2d::core::step_from_rate(tuning::tick_rate));
        if (!rtype::snapshot_due(tick)) {
            continue;
        }
        const Snapshot snapshot = rtype::take_snapshot(instance);
        const Snapshot* const base = sent.size() >= 2 ? &sent.at(sent.size() - 2) : nullptr;
        const Sent delta = send(snapshot, base);
        REQUIRE(delta.rebuilt);
        CHECK(delta.rebuilt == expected(snapshot));
        for (const std::vector<std::byte>& payload : delta.payloads) {
            delta_bytes += payload.size();
        }
        for (const std::vector<std::byte>& payload : rtype::write_snapshot(snapshot)) {
            full_bytes += payload.size();
        }
        // What the client has: the snapshot as rebuilt, the next baseline.
        sent.push_back(delta.rebuilt.value_or(Snapshot{}));
    }
    CHECK(delta_bytes < full_bytes);
}
