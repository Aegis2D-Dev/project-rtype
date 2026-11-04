#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/entities.hpp>
#include <rtype/layers.hpp>
#include <rtype/position_history.hpp>
#include <rtype/server_game.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

using aegis2d::Vec2;
using aegis2d::core::step_from_rate;
using aegis2d::ecs::Collider;
using aegis2d::ecs::Entity;
using aegis2d::ecs::Registry;
using aegis2d::ecs::Transform;
using rtype::PositionHistory;
using rtype::ServerGame;
namespace tuning = rtype::tuning;

TEST_CASE("the server reads where an enemy was at a past tick", "[server][history]") {
    // No seed, no waves: a Bydos alone, weaving left, and a player.
    ServerGame game;
    static_cast<void>(game.add_player());
    const Entity bydos = rtype::spawn_bydos_slave(game.registry(), {1200.0F, 300.0F});
    std::map<std::uint64_t, Vec2> seen;
    for (int tick = 0; tick < 2 * static_cast<int>(tuning::tick_rate); ++tick) {
        game.tick(step_from_rate(tuning::tick_rate));
        seen[game.ticks()] = game.registry().get<Transform>(bydos).position;
    }
    const PositionHistory& history = game.history();
    const std::uint64_t now = game.ticks();
    REQUIRE(history.size() == PositionHistory::capacity);
    CHECK(history.newest() == std::optional{now});
    CHECK(history.oldest() == std::optional{now - (PositionHistory::capacity - 1)});

    // Each of the last 500 ms, exactly as the tick left it; the Bydos moved meanwhile.
    for (std::uint64_t tick = now - (PositionHistory::capacity - 1); tick <= now; ++tick) {
        CAPTURE(tick);
        CHECK(history.position(bydos, tick) == std::optional{seen.at(tick)});
    }
    CHECK(history.position(bydos, now - 20) != history.position(bydos, now));
    // Older ticks are forgotten, later ones not there yet.
    CHECK(!history.position(bydos, now - PositionHistory::capacity).has_value());
    CHECK(!history.position(bydos, now + 1).has_value());
}

TEST_CASE("only the enemies are kept, each while it lives", "[server][history]") {
    ServerGame game;
    const Entity player = game.add_player();
    const Entity first = rtype::spawn_bydos_slave(game.registry(), {1000.0F, 200.0F});
    const Entity second = rtype::spawn_bydos_slave(game.registry(), {1100.0F, 500.0F});
    game.tick(step_from_rate(tuning::tick_rate));
    const std::uint64_t both = game.ticks();
    CHECK(!game.history().position(player, both).has_value());
    REQUIRE(game.history().enemies(both).size() == 2);
    CHECK(game.history().enemies(both).front().first.id() <
          game.history().enemies(both).back().first.id());

    // Gone: still at the ticks it lived, not after.
    game.registry().destroy(first);
    game.tick(step_from_rate(tuning::tick_rate));
    const std::uint64_t after = game.ticks();
    CHECK(game.history().position(first, both).has_value());
    CHECK(!game.history().position(first, after).has_value());
    CHECK(game.history().position(second, after).has_value());
    CHECK(game.history().enemies(after).size() == 1);
    // A new entity in the freed slot is another enemy, with no past.
    const Entity reused = rtype::spawn_bydos_slave(game.registry(), {900.0F, 360.0F});
    CHECK(!game.history().position(reused, both).has_value());
}

TEST_CASE("the history starts again with a new game", "[server][history]") {
    Registry world;
    const Entity enemy = world.create();
    world.emplace<Transform>(enemy, Vec2{10.0F, 20.0F});
    world.emplace<Collider>(
        enemy,
        Collider{.size = {8.0F, 8.0F}, .layer = rtype::layer::enemy, .mask = rtype::mask::enemy});
    PositionHistory history;
    CHECK(!history.oldest().has_value());
    CHECK(!history.position(enemy, 0).has_value());
    CHECK(history.enemies(0).empty());
    for (std::uint64_t tick = 1; tick <= 100; ++tick) {
        world.get<Transform>(enemy).position.x = static_cast<float>(tick);
        history.record(tick, world);
    }
    CHECK(history.size() == PositionHistory::capacity);
    CHECK(history.position(enemy, 80) == std::optional{Vec2{80.0F, 20.0F}});

    // A tick not newer than the last: a new game, from scratch.
    history.record(3, world);
    CHECK(history.size() == 1);
    CHECK(history.oldest() == std::optional<std::uint64_t>{3});
    CHECK(!history.position(enemy, 80).has_value());
    history.clear();
    CHECK(history.size() == 0);
}
