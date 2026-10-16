#include <aegis2d/core/clock.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/waves.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/entities.hpp>
#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using aegis2d::Vec2;
using aegis2d::core::step_from_rate;
using aegis2d::ecs::Entity;
using aegis2d::ecs::SineMotion;
using aegis2d::ecs::Transform;
using aegis2d::ecs::WaveSchedule;
using rtype::DeathEvent;
using rtype::DestroyEvent;
using rtype::DestroyReason;
using rtype::EntityKind;
using rtype::EventTracker;
using rtype::GameEvent;
using rtype::GameInstance;
using rtype::LeaveReason;
using rtype::PlayerJoinedEvent;
using rtype::PlayerLeftEvent;
using rtype::SpawnEvent;
namespace tuning = rtype::tuning;

namespace {

std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for (const unsigned value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

std::uint8_t kind(EntityKind value) {
    return static_cast<std::uint8_t>(value);
}

// The events of one type, in order.
template <typename Event>
std::vector<Event> only(const std::vector<GameEvent>& events) {
    std::vector<Event> result;
    for (const GameEvent& event : events) {
        if (const auto* const found = std::get_if<Event>(&event)) {
            result.push_back(*found);
        }
    }
    return result;
}

// The events of the game's next tick.
std::vector<GameEvent> tick(GameInstance& instance, EventTracker& tracker) {
    instance.game().tick(step_from_rate(tuning::tick_rate));
    return tracker.update(instance);
}

Vec2 position_of(GameInstance& instance, Entity entity) {
    return instance.game().registry().get<Transform>(entity).position;
}

// A Bydos that does not weave, for the tests that aim a missile at it.
Entity straight_bydos(GameInstance& instance, Vec2 position) {
    const Entity bydos = rtype::spawn_bydos_slave(instance.game().registry(), position);
    instance.game().registry().remove<SineMotion>(bydos);
    return bydos;
}

const SpawnEvent spawn_example{
    .tick = 600, .id = 2, .kind = 1, .x = 100.0F, .y = 360.0F, .vx = 0.0F, .vy = 0.0F, .owner = 0};
const DestroyEvent destroy_example{
    .tick = 601, .id = 7, .reason = DestroyReason::Killed, .x = 640.0F, .y = 200.0F};
const DeathEvent death_example{.tick = 602, .player = 3, .lives = 2, .x = 50.0F, .y = 25.0F};
PlayerJoinedEvent joined_example() {
    return PlayerJoinedEvent{.tick = 603, .player = 1, .name = "R9"};
}
const PlayerLeftEvent left_example{.tick = 604, .player = 2, .reason = LeaveReason::TimedOut};

} // namespace

TEST_CASE("events follow the RFC layout byte by byte", "[server][events]") {
    // RFC section 4.4: type, tick, then the fields of the type, little-endian.
    CHECK(rtype::write_event(spawn_example) == bytes({
                                                   0x01,                   // type     Spawn
                                                   0x58, 0x02, 0x00, 0x00, // tick     600
                                                   0x02, 0x00, 0x00, 0x00, // id       2
                                                   0x01,                   // kind     PlayerShip
                                                   0x00, 0x00, 0xC8, 0x42, // x        100.0
                                                   0x00, 0x00, 0xB4, 0x43, // y        360.0
                                                   0x00, 0x00, 0x00, 0x00, // vx       0.0
                                                   0x00, 0x00, 0x00, 0x00, // vy       0.0
                                                   0x00,                   // owner    player 0
                                               }));
    CHECK(rtype::write_event(destroy_example) == bytes({
                                                     0x02, // type   Destroy
                                                     0x59,
                                                     0x02,
                                                     0x00,
                                                     0x00, // tick   601
                                                     0x07,
                                                     0x00,
                                                     0x00,
                                                     0x00, // id     7
                                                     0x01, // reason Killed
                                                     0x00,
                                                     0x00,
                                                     0x20,
                                                     0x44, // x      640.0
                                                     0x00,
                                                     0x00,
                                                     0x48,
                                                     0x43, // y      200.0
                                                 }));
    CHECK(rtype::write_event(death_example) == bytes({
                                                   0x03, // type   Death
                                                   0x5A,
                                                   0x02,
                                                   0x00,
                                                   0x00, // tick   602
                                                   0x03, // player 3
                                                   0x02, // lives  2
                                                   0x00,
                                                   0x00,
                                                   0x48,
                                                   0x42, // x      50.0
                                                   0x00,
                                                   0x00,
                                                   0xC8,
                                                   0x41, // y      25.0
                                               }));
    CHECK(rtype::write_event(spawn_example).size() == rtype::spawn_event_size);
    CHECK(rtype::write_event(destroy_example).size() == rtype::destroy_event_size);
    CHECK(rtype::write_event(death_example).size() == rtype::death_event_size);
}

TEST_CASE("events read back as they were written", "[server][events]") {
    for (const GameEvent& event :
         {GameEvent{spawn_example}, GameEvent{destroy_example}, GameEvent{death_example},
          GameEvent{joined_example()}, GameEvent{left_example}}) {
        CHECK(rtype::read_event(rtype::write_event(event)) == event);
    }
    // Unknown kinds and reasons are not errors: the client applies their rules (4.1, 4.4).
    SpawnEvent unknown_kind = spawn_example;
    unknown_kind.kind = 9;
    CHECK(rtype::read_event(rtype::write_event(unknown_kind)) == GameEvent{unknown_kind});
    std::vector<std::byte> unknown_reason = rtype::write_event(destroy_example);
    unknown_reason.at(9) = std::byte{7}; // reason, after type, tick and id
    const std::optional<GameEvent> read_reason = rtype::read_event(unknown_reason);
    REQUIRE(read_reason.has_value());
    const GameEvent reason_event = read_reason.value_or(GameEvent{});
    const auto* const destroy = std::get_if<DestroyEvent>(&reason_event);
    REQUIRE(destroy != nullptr);
    CHECK(std::to_underlying(destroy->reason) == 7);
    SpawnEvent enemy = spawn_example;
    enemy.owner = rtype::no_player;
    CHECK(rtype::read_event(rtype::write_event(enemy)) == GameEvent{enemy});
}

TEST_CASE("malformed events are rejected", "[server][events]") {
    // Every truncation, and a byte too many (RFC section 6.4).
    for (const GameEvent& event :
         {GameEvent{spawn_example}, GameEvent{destroy_example}, GameEvent{death_example},
          GameEvent{joined_example()}, GameEvent{left_example}}) {
        const std::vector<std::byte> valid = rtype::write_event(event);
        for (std::size_t size = 0; size < valid.size(); ++size) {
            CAPTURE(size);
            CHECK(!rtype::read_event(std::span{valid}.first(size)));
        }
        std::vector<std::byte> longer = valid;
        longer.push_back(std::byte{0});
        CHECK(!rtype::read_event(longer));
    }
    // GameOver, which this server never sends, and the reserved types.
    for (const unsigned type : {0U, 6U, 7U, 255U}) {
        std::vector<std::byte> event = rtype::write_event(death_example);
        event.front() = static_cast<std::byte>(type);
        CAPTURE(type);
        CHECK(!rtype::read_event(event));
    }
    SpawnEvent not_finite = spawn_example;
    not_finite.vx = std::numeric_limits<float>::infinity();
    CHECK(!rtype::read_event(rtype::write_event(not_finite)));
    SpawnEvent bad_owner = spawn_example;
    bad_owner.owner = 4;
    CHECK(!rtype::read_event(rtype::write_event(bad_owner)));
    DestroyEvent nan_position = destroy_example;
    nan_position.y = std::numeric_limits<float>::quiet_NaN();
    CHECK(!rtype::read_event(rtype::write_event(nan_position)));
    DeathEvent bad_player = death_example;
    bad_player.player = 4;
    CHECK(!rtype::read_event(rtype::write_event(bad_player)));
}

TEST_CASE("ships, enemies and shots are spawned with their owner", "[server][events]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    EventTracker tracker;
    const Entity ship = instance.join(1);
    const Entity bydos = straight_bydos(instance, {1000.0F, 100.0F});

    const std::vector<SpawnEvent> first = only<SpawnEvent>(tick(instance, tracker));
    REQUIRE(first.size() == 2);
    for (const SpawnEvent& spawn : first) {
        const bool is_ship = spawn.id == ship.id();
        CHECK(spawn.tick == 1);
        CHECK(spawn.kind == kind(is_ship ? EntityKind::PlayerShip : EntityKind::BydosSlave));
        CHECK(spawn.owner == (is_ship ? 1 : rtype::no_player));
        const Vec2 position = position_of(instance, is_ship ? ship : bydos);
        CHECK(spawn.x == position.x);
        CHECK(spawn.y == position.y);
    }
    CHECK(first.at(0).id != first.at(1).id);
    CHECK(tracker.tracked() == 2);

    // Moving is for the snapshots: no event.
    CHECK(tick(instance, tracker).empty());

    // The shot is the Spawn of the missile, owned by the shooter's player.
    const Entity missile = rtype::spawn_player_missile(instance.game().registry(), ship);
    const std::vector<GameEvent> shot = tick(instance, tracker);
    REQUIRE(shot.size() == 1);
    const auto* const spawn = std::get_if<SpawnEvent>(&shot.front());
    REQUIRE(spawn != nullptr);
    CHECK(spawn->id == missile.id());
    CHECK(spawn->kind == kind(EntityKind::PlayerMissile));
    CHECK(spawn->owner == 1);
    CHECK(spawn->x == position_of(instance, missile).x);
    CHECK(spawn->vx == tuning::player_missile_speed);
}

TEST_CASE("a missile that kills a Bydos destroys both where they were", "[server][events]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    EventTracker tracker;
    const Entity ship = instance.join(0);
    const Vec2 nose = position_of(instance, ship);
    const Entity bydos = straight_bydos(instance, {nose.x + 300.0F, nose.y});
    static_cast<void>(tick(instance, tracker));
    const Entity missile = rtype::spawn_player_missile(instance.game().registry(), ship);

    // The positions of the last tick before the hit are the ones sent.
    std::vector<GameEvent> events;
    Vec2 bydos_before{};
    Vec2 missile_before{};
    for (int ticks = 0; ticks < 60 && instance.game().registry().alive(bydos); ++ticks) {
        bydos_before = position_of(instance, bydos);
        if (instance.game().registry().alive(missile)) {
            missile_before = position_of(instance, missile);
        }
        events = tick(instance, tracker);
    }
    REQUIRE(!instance.game().registry().alive(bydos));
    const std::vector<DestroyEvent> destroyed = only<DestroyEvent>(events);
    REQUIRE(destroyed.size() == 2);
    for (const DestroyEvent& destroy : destroyed) {
        const bool is_bydos = destroy.id == bydos.id();
        CHECK(destroy.tick == instance.game().ticks());
        CHECK(destroy.reason == (is_bydos ? DestroyReason::Killed : DestroyReason::Consumed));
        CHECK(destroy.x == (is_bydos ? bydos_before : missile_before).x);
        CHECK(destroy.y == (is_bydos ? bydos_before : missile_before).y);
    }
    CHECK(tracker.tracked() == 1); // the ship
}

TEST_CASE("each death comes where the ship was, the last one before its Destroy",
          "[server][events]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    EventTracker tracker;
    const Entity ship = instance.join(2);
    static_cast<void>(tick(instance, tracker));

    for (std::uint32_t life = tuning::player_lives; life > 0; --life) {
        const Vec2 position = position_of(instance, ship);
        // Hit by an enemy missile, spent in the same tick: the missile itself is never seen.
        rtype::spawn_enemy_missile(instance.game().registry(), position);
        const std::vector<GameEvent> events = tick(instance, tracker);
        REQUIRE(!events.empty());
        const auto* const death = std::get_if<DeathEvent>(&events.front());
        REQUIRE(death != nullptr);
        CHECK(*death == DeathEvent{.tick = static_cast<std::uint32_t>(instance.game().ticks()),
                                   .player = 2,
                                   .lives = static_cast<std::uint8_t>(life - 1),
                                   .x = position.x,
                                   .y = position.y});
        if (life > 1) {
            CHECK(events.size() == 1); // respawned: the same ship goes on
        } else {
            REQUIRE(events.size() == 2);
            const auto* const destroy = std::get_if<DestroyEvent>(&events.back());
            REQUIRE(destroy != nullptr);
            CHECK(destroy->id == ship.id());
            CHECK(destroy->reason == DestroyReason::Killed);
        }
    }
}

TEST_CASE("a missile leaving the screen is despawned", "[server][events]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    EventTracker tracker;
    const Entity ship = instance.join(0);
    const Entity missile = rtype::spawn_player_missile(instance.game().registry(), ship);
    static_cast<void>(tick(instance, tracker));

    std::vector<GameEvent> events;
    for (int ticks = 0; ticks < 240 && instance.game().registry().alive(missile); ++ticks) {
        events = tick(instance, tracker);
    }
    REQUIRE(!instance.game().registry().alive(missile));
    REQUIRE(events.size() == 1);
    const auto* const destroy = std::get_if<DestroyEvent>(&events.front());
    REQUIRE(destroy != nullptr);
    CHECK(destroy->id == missile.id());
    CHECK(destroy->reason == DestroyReason::Despawned);
}

TEST_CASE("the ship of a player who leaves is destroyed for the others", "[server][events]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    EventTracker tracker;
    static_cast<void>(instance.join(0));
    const Entity leaving = instance.join(3);
    static_cast<void>(tick(instance, tracker));

    REQUIRE(instance.leave(3));
    const std::vector<GameEvent> events = tick(instance, tracker);
    REQUIRE(events.size() == 1);
    const auto* const destroy = std::get_if<DestroyEvent>(&events.front());
    REQUIRE(destroy != nullptr);
    CHECK(destroy->id == leaving.id());
    CHECK(destroy->reason == DestroyReason::PlayerLeft);

    // The last player leaving starts a new game: the old one is forgotten, without events.
    REQUIRE(instance.leave(0));
    CHECK(tick(instance, tracker).empty());
    CHECK(tracker.tracked() == 0);
}

TEST_CASE("a player who joins gets a Spawn of every entity alive", "[server][events]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    EventTracker tracker;
    const Entity first = instance.join(0);
    const Entity bydos = straight_bydos(instance, {1000.0F, 100.0F});
    static_cast<void>(tick(instance, tracker));
    static_cast<void>(tick(instance, tracker));

    // As the server does when a player joins: join, then the world so far.
    const Entity second = instance.join(1);
    const std::vector<SpawnEvent> world = only<SpawnEvent>(tracker.spawns(instance));
    REQUIRE(world.size() == 2);
    for (const SpawnEvent& spawn : world) {
        const bool is_ship = spawn.id == first.id();
        CHECK((is_ship || spawn.id == bydos.id()));
        CHECK(spawn.tick == 2);
        CHECK(spawn.owner == (is_ship ? 0 : rtype::no_player));
        CHECK(spawn.x == position_of(instance, is_ship ? first : bydos).x);
    }

    // Its own ship comes with the next tick, to every player.
    const std::vector<SpawnEvent> next = only<SpawnEvent>(tick(instance, tracker));
    REQUIRE(next.size() == 1);
    CHECK(next.front().id == second.id());
    CHECK(next.front().owner == 1);
}

TEST_CASE("players joining and leaving follow the RFC layout", "[server][events]") {
    CHECK(rtype::write_event(joined_example()) == bytes({
                                                      0x04, // type   PlayerJoined
                                                      0x5B,
                                                      0x02,
                                                      0x00,
                                                      0x00, // tick   603
                                                      0x01, // player 1
                                                      0x02,
                                                      0x00,
                                                      'R',
                                                      '9', // name   "R9"
                                                  }));
    CHECK(rtype::write_event(left_example) == bytes({
                                                  0x05, // type   PlayerLeft
                                                  0x5C,
                                                  0x02,
                                                  0x00,
                                                  0x00, // tick   604
                                                  0x02, // player 2
                                                  0x01, // reason TimedOut
                                              }));
    CHECK(rtype::write_event(left_example).size() == rtype::player_left_event_size);
    CHECK(rtype::write_event(joined_example()).size() == 8 + joined_example().name.size());

    // An unknown reason is kept: the client reads it as Left (RFC section 4.4).
    std::vector<std::byte> unknown_reason = rtype::write_event(left_example);
    unknown_reason.back() = std::byte{9};
    const GameEvent left = rtype::read_event(unknown_reason).value_or(GameEvent{});
    const auto* const read = std::get_if<PlayerLeftEvent>(&left);
    REQUIRE(read != nullptr);
    CHECK(std::to_underlying(read->reason) == 9);

    // A name that breaks the rules of LoginRequest, or a player above 3 (RFC section 6.4).
    for (const std::string& name :
         {std::string{}, std::string(17, 'x'), std::string{"a\nb"}, std::string{"\xFF"}}) {
        PlayerJoinedEvent bad_name = joined_example();
        bad_name.name = name;
        CHECK(!rtype::read_event(rtype::write_event(bad_name)));
    }
    PlayerJoinedEvent bad_player = joined_example();
    bad_player.player = 4;
    CHECK(!rtype::read_event(rtype::write_event(bad_player)));
    PlayerLeftEvent bad_left = left_example;
    bad_left.player = 255;
    CHECK(!rtype::read_event(rtype::write_event(bad_left)));
}
