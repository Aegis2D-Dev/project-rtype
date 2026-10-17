#include <aegis2d/core/clock.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/packet.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/entities.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/simulation.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>
#include <rtype/world_snapshot.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
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

TEST_CASE("a snapshot follows the RFC example byte by byte", "[server][snapshot]") {
    // RFC section 4.7.
    const std::vector<std::vector<std::byte>> payloads = rtype::write_snapshot(rfc_example());
    REQUIRE(payloads.size() == 1);
    CHECK(payloads.front() ==
          bytes({0x58, 0x02, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x64, 0x00,
                 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xC8, 0x42,
                 0x00, 0x00, 0xB4, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
    CHECK(rtype::read_snapshot(payloads.front()) == std::optional{rfc_example()});
}

TEST_CASE("a large world is split into datagrams of 51 entities", "[server][snapshot]") {
    CHECK(rtype::max_entities_per_datagram == 51);
    Snapshot snapshot{.tick = 7, .players = {}, .entities = {}};
    for (std::uint8_t player = 0; player < tuning::max_players; ++player) {
        snapshot.players.push_back(PlayerRecord{.player = player, .ship = player, .lives = 1});
    }
    for (std::uint32_t id = 0; id < 120; ++id) {
        snapshot.entities.push_back(EntityRecord{.id = id, .kind = 3, .x = static_cast<float>(id)});
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
    CHECK(payloads.at(0).size() == aegis2d::net::PacketHeader::max_payload_size - 13);
    CHECK(parsed(payloads.at(2)).entities.size() == 18);
    CHECK(entities == snapshot.entities);

    // An empty world still gives one datagram, with the players.
    const Snapshot empty{.tick = 1, .players = snapshot.players, .entities = {}};
    REQUIRE(rtype::write_snapshot(empty).size() == 1);
    CHECK(rtype::read_snapshot(rtype::write_snapshot(empty).front()) == std::optional{empty});
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

    std::vector<std::byte> bad_player = valid;
    bad_player.at(5) = std::byte{4}; // player 4
    CHECK(!rtype::read_snapshot(bad_player).has_value());

    Snapshot five = rfc_example();
    five.players.resize(5);
    CHECK(!rtype::read_snapshot(rtype::write_snapshot(five).front()).has_value());

    Snapshot not_finite = rfc_example();
    not_finite.entities.front().vx = std::numeric_limits<float>::infinity();
    CHECK(!rtype::read_snapshot(rtype::write_snapshot(not_finite).front()).has_value());
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
