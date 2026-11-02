#include <aegis2d/core/clock.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/bits.hpp>
#include <aegis2d/net/packet.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/compact.hpp>
#include <rtype/entities.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/simulation.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>
#include <rtype/waves.hpp>
#include <rtype/world_snapshot.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

using aegis2d::core::step_from_rate;
using aegis2d::ecs::Entity;
using aegis2d::ecs::Score;
using aegis2d::ecs::Transform;
using aegis2d::ecs::WaveSchedule;
using rtype::EntityKind;
using rtype::EntityRecord;
using rtype::GameInstance;
using rtype::PlayerRecord;
using rtype::Snapshot;
namespace tuning = rtype::tuning;

namespace {

std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for (const unsigned value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

// The snapshot of a payload, which must be well formed.
Snapshot parsed(std::span<const std::byte> payload) {
    const std::optional<Snapshot> read = rtype::read_snapshot(payload);
    REQUIRE(read.has_value());
    return read.value_or(Snapshot{});
}

Snapshot rfc_example() {
    return Snapshot{
        .tick = 600,
        .players = {PlayerRecord{.player = 0, .ship = 2, .lives = 3, .score = 100}},
        .entities = {EntityRecord{.id = 2,
                                  .kind = static_cast<std::uint8_t>(EntityKind::PlayerShip),
                                  .x = 100.0F,
                                  .y = 360.0F,
                                  .vx = 0.0F,
                                  .vy = 0.0F,
                                  .frame = 0}},
    };
}

} // namespace

TEST_CASE("snapshots are due 20 times per second", "[server][snapshot]") {
    std::size_t due = 0;
    for (std::uint64_t tick = 1; tick <= tuning::tick_rate; ++tick) {
        due += rtype::snapshot_due(tick) ? 1 : 0;
    }
    CHECK(due == tuning::snapshot_rate);
    CHECK(rtype::snapshot_due(3));
    CHECK(!rtype::snapshot_due(4));
}

TEST_CASE("a snapshot follows the RFC example bit by bit", "[server][snapshot]") {
    // RFC section 4.7: 155 bits, the entity record 77 of them.
    const std::vector<std::vector<std::byte>> payloads = rtype::write_snapshot(rfc_example());
    REQUIRE(payloads.size() == 1);
    CHECK(payloads.front() == bytes({0x58, 0x02, 0x00, 0x00, 0xA1, 0xC0, 0x00, 0x59, 0x00, 0x80,
                                     0x00, 0x40, 0x40, 0x06, 0x40, 0x0B, 0x00, 0x00, 0x00, 0x00}));
    CHECK(rtype::read_snapshot(payloads.front()) == std::optional{rfc_example()});
    CHECK(rtype::entity_record_bits(2) == 77);
}

TEST_CASE("an entity record takes at most 10 bytes", "[server][snapshot]") {
    CHECK(rtype::entity_record_fixed_bits == 69);
    CHECK(rtype::entity_record_bits(0) == 77);
    CHECK(rtype::entity_record_bits(rtype::short_id_gap) == 77);
    CHECK(rtype::entity_record_bits(rtype::short_id_gap + 1) == 85);

    // The generation in the 8 high bits of an id costs nothing more: index 2 + 127 of the
    // 255th generation, after index 2, adds 77 bits, within 10 bytes.
    Snapshot snapshot = rfc_example();
    const std::size_t one = rtype::write_snapshot(snapshot).front().size();
    snapshot.entities.push_back(
        EntityRecord{.id = 0xFF000000U | (2 + rtype::short_id_gap), .kind = 4});
    const std::vector<std::byte> two = rtype::write_snapshot(snapshot).front();
    CHECK(two.size() - one <= 10);
    CHECK(parsed(two).entities.back() == snapshot.entities.back());
}

TEST_CASE("snapshots of a game are drawn within half a pixel", "[server][snapshot]") {
    GameInstance instance{42, rtype::load_waves(RTYPE_ASSET_DIR "/waves/stage1.json")};
    for (std::uint8_t player = 0; player < tuning::max_players; ++player) {
        instance.join(player);
        aegis2d::ecs::PlayerInput input;
        input.set(aegis2d::ecs::Action::Fire, true);
        input.set(player % 2 == 0 ? aegis2d::ecs::Action::Up : aegis2d::ecs::Action::Right, true);
        instance.set_input(player, input);
    }
    std::size_t records = 0;
    std::optional<Snapshot> last_sent;
    std::optional<Snapshot> last_read;
    for (std::uint64_t tick = 1; tick <= 20ULL * tuning::tick_rate; ++tick) {
        instance.game().tick(step_from_rate(tuning::tick_rate));
        if (!rtype::snapshot_due(tick)) {
            continue;
        }
        Snapshot sent = rtype::take_snapshot(instance);
        std::ranges::sort(sent.entities, {}, [](const EntityRecord& entity) {
            return ((entity.id & 0xFFFFFFULL) << 32U) | entity.id;
        });
        Snapshot read{.tick = sent.tick, .players = sent.players, .entities = {}};
        for (const std::vector<std::byte>& payload : rtype::write_snapshot(sent)) {
            const Snapshot part = parsed(payload);
            CHECK(part.players == sent.players);
            std::uint32_t previous = 0;
            for (const EntityRecord& entity : part.entities) {
                const std::uint32_t low = entity.id & 0xFFFFFFU;
                CHECK(rtype::entity_record_bits(low - previous) <= 80);
                previous = low;
            }
            read.entities.insert(read.entities.end(), part.entities.begin(), part.entities.end());
        }
        REQUIRE(read.entities.size() == sent.entities.size());
        for (std::size_t index = 0; index < sent.entities.size(); ++index) {
            const EntityRecord& server = sent.entities.at(index);
            const EntityRecord& client = read.entities.at(index);
            CHECK(client.id == server.id);
            CHECK(client.kind == server.kind);
            CHECK(std::fabs(client.x - server.x) <= rtype::position_max_error);
            CHECK(std::fabs(client.y - server.y) <= rtype::position_max_error);
            CHECK(std::fabs(client.vx - server.vx) <= rtype::velocity_max_error);
            CHECK(std::fabs(client.vy - server.vy) <= rtype::velocity_max_error);
        }
        // Drawn between two snapshots, a position mixes two received ones: its error does
        // not exceed theirs.
        if (last_sent && last_read && !sent.entities.empty() && !last_sent->entities.empty() &&
            last_sent->entities.front().id == sent.entities.front().id) {
            for (const float t : {0.25F, 0.5F, 0.75F}) {
                const float server =
                    std::lerp(last_sent->entities.front().x, sent.entities.front().x, t);
                const float client =
                    std::lerp(last_read->entities.front().x, read.entities.front().x, t);
                CHECK(std::fabs(client - server) < 0.5F);
            }
        }
        records += sent.entities.size();
        last_sent = sent;
        last_read = read;
    }
    CHECK(records > 1000);
}

TEST_CASE("a large world is split into datagrams", "[server][snapshot]") {
    Snapshot snapshot{.tick = 7, .players = {}, .entities = {}};
    for (std::uint8_t player = 0; player < tuning::max_players; ++player) {
        snapshot.players.push_back(PlayerRecord{.player = player, .ship = player, .lives = 1});
    }
    // In reverse order: the records are sent by increasing id.
    for (std::uint32_t id = 300; id > 0; --id) {
        snapshot.entities.push_back(
            EntityRecord{.id = id - 1, .kind = 3, .x = static_cast<float>(id)});
    }

    const std::vector<std::vector<std::byte>> payloads = rtype::write_snapshot(snapshot);
    REQUIRE(payloads.size() == 3);
    std::vector<EntityRecord> entities;
    for (const std::vector<std::byte>& payload : payloads) {
        CHECK(payload.size() <= aegis2d::net::PacketHeader::max_payload_size);
        const Snapshot part = parsed(payload);
        CHECK(part.tick == 7);
        CHECK(part.players == snapshot.players);
        entities.insert(entities.end(), part.entities.begin(), part.entities.end());
    }
    // 51 bits, 4 players of 27 bits, then 120 records of 77 bits: 9399 of the 9456 bits of
    // a payload.
    CHECK(payloads.at(0).size() == 1175);
    CHECK(parsed(payloads.at(0)).entities.size() == 120);
    CHECK(parsed(payloads.at(2)).entities.size() == 60);
    std::ranges::reverse(snapshot.entities);
    CHECK(entities == snapshot.entities);

    // An empty world still gives one datagram, with the players.
    const Snapshot empty{.tick = 1, .players = snapshot.players, .entities = {}};
    REQUIRE(rtype::write_snapshot(empty).size() == 1);
    CHECK(rtype::read_snapshot(rtype::write_snapshot(empty).front()) == std::optional{empty});
}

TEST_CASE("snapshot values out of their fields are clamped or refused", "[server][snapshot]") {
    Snapshot snapshot = rfc_example();
    snapshot.players.front().ship = rtype::no_entity;
    snapshot.players.front().score = std::numeric_limits<std::uint32_t>::max();
    EntityRecord& entity = snapshot.entities.front();
    entity.id = std::numeric_limits<std::uint32_t>::max();
    entity.x = std::numeric_limits<float>::infinity();
    entity.y = -5000.0F;
    entity.vx = std::numeric_limits<float>::quiet_NaN();
    entity.vy = 1000.4F;
    entity.frame = 200;
    const Snapshot read = parsed(rtype::write_snapshot(snapshot).front());
    CHECK(read.players == snapshot.players);
    CHECK(read.entities.front() == EntityRecord{.id = entity.id,
                                                .kind = entity.kind,
                                                .x = rtype::position_max,
                                                .y = rtype::position_min,
                                                .vx = 0.0F,
                                                .vy = 1000.0F,
                                                .frame = 15});

    Snapshot bad_kind = rfc_example();
    bad_kind.entities.front().kind = 8;
    CHECK_THROWS_AS(rtype::write_snapshot(bad_kind), std::invalid_argument);
    Snapshot bad_player = rfc_example();
    bad_player.players.front().player = 4;
    CHECK_THROWS_AS(rtype::write_snapshot(bad_player), std::invalid_argument);
}

TEST_CASE("malformed snapshots are rejected", "[server][snapshot]") {
    const std::vector<std::byte> valid = rtype::write_snapshot(rfc_example()).front();
    std::vector<std::byte> trailing = valid;
    trailing.push_back(std::byte{0});
    CHECK(!rtype::read_snapshot(trailing).has_value());
    for (std::size_t size = 0; size < valid.size(); ++size) {
        CAPTURE(size);
        CHECK(!rtype::read_snapshot(std::span{valid}.first(size)).has_value());
    }
    // The 5 bits after the 155 of the example are padding, and must be 0.
    std::vector<std::byte> padding = valid;
    padding.back() |= std::byte{0x08};
    CHECK(!rtype::read_snapshot(padding).has_value());

    Snapshot five = rfc_example();
    five.players.resize(5);
    CHECK(!rtype::read_snapshot(rtype::write_snapshot(five).front()).has_value());

    // The ship of the example as a varint with a last group of 0: 2, then 0.
    aegis2d::net::BitWriter overlong;
    overlong.write_bits(600, 32);
    overlong.write_bits(1, 3);
    overlong.write_bits(0, 2);
    overlong.write_bool(true);
    overlong.write_bits(2, 7);
    overlong.write_bool(true);
    overlong.write_bits(0, 7);
    overlong.write_bool(false);
    overlong.write_bits(3, 8);
    rtype::write_varint(overlong, 100);
    overlong.write_bits(0, 16);
    CHECK(!rtype::read_snapshot(overlong.data()).has_value());

    // Gaps whose sum goes past the 24 low bits of an id: alone, then with the one before.
    for (const std::uint32_t first : {std::uint32_t{1} << 24U, (std::uint32_t{1} << 24U) - 1}) {
        aegis2d::net::BitWriter past_24_bits;
        past_24_bits.write_bits(600, 32);
        past_24_bits.write_bits(0, 3);
        past_24_bits.write_bits(2, 16);
        for (const std::uint32_t gap : {first, std::uint32_t{1}}) {
            rtype::write_varint(past_24_bits, gap);
            past_24_bits.write_bits(0, 64);
            past_24_bits.write_bits(0, rtype::entity_record_fixed_bits - 64);
        }
        CHECK(!rtype::read_snapshot(past_24_bits.data()).has_value());
    }
}

TEST_CASE("the snapshot holds the players and the entities of the game", "[server][snapshot]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    const Entity ship = instance.join(2);
    instance.game().registry().get<Score>(ship).points = 300;
    const Entity bydos = rtype::spawn_bydos_slave(instance.game().registry(), {900.0F, 200.0F});
    instance.game().tick(step_from_rate(tuning::tick_rate));

    const Snapshot snapshot = rtype::take_snapshot(instance);
    CHECK(snapshot.tick == 1);
    REQUIRE(snapshot.players.size() == 1);
    CHECK(
        snapshot.players.front() ==
        PlayerRecord{.player = 2, .ship = ship.id(), .lives = tuning::player_lives, .score = 300});
    REQUIRE(snapshot.entities.size() == 2);
    for (const EntityRecord& entity : snapshot.entities) {
        const Entity handle = entity.id == ship.id() ? ship : bydos;
        CHECK(entity.kind == static_cast<std::uint8_t>(handle == ship ? EntityKind::PlayerShip
                                                                      : EntityKind::BydosSlave));
        const auto position = instance.game().registry().get<Transform>(handle).position;
        CHECK(entity.x == position.x);
        CHECK(entity.y == position.y);
    }
    CHECK(snapshot.entities.at(0).id != snapshot.entities.at(1).id);

    // Out of lives: no ship, no lives, the last score.
    for (std::uint32_t life = 0; life < tuning::player_lives; ++life) {
        const auto position = instance.game().registry().get<Transform>(ship).position;
        rtype::spawn_enemy_missile(instance.game().registry(), position);
        instance.game().tick(step_from_rate(tuning::tick_rate));
    }
    const Snapshot after = rtype::take_snapshot(instance);
    REQUIRE(after.players.size() == 1);
    CHECK(after.players.front() ==
          PlayerRecord{.player = 2, .ship = rtype::no_entity, .lives = 0, .score = 300});
}

TEST_CASE("the tick handler runs after each tick on the simulation thread",
          "[server][simulation]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> last_tick{0};
    rtype::Simulation simulation{instance, tuning::tick_rate, [&](GameInstance& game) {
                                     calls.fetch_add(1);
                                     last_tick.store(game.game().ticks());
                                 }};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (calls.load() < 10 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    simulation.stop();
    CHECK(calls.load() >= 10);
    CHECK(calls.load() == simulation.ticks());
    CHECK(last_tick.load() == simulation.ticks());
}
