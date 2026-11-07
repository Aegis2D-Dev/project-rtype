#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/entities.hpp>
#include <rtype/lag_compensation.hpp>
#include <rtype/layers.hpp>
#include <rtype/position_history.hpp>
#include <rtype/server_game.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>

using aegis2d::Vec2;
using aegis2d::core::step_from_rate;
using aegis2d::ecs::Collider;
using aegis2d::ecs::Entity;
using aegis2d::ecs::Health;
using aegis2d::ecs::Transform;
using aegis2d::ecs::Velocity;
using rtype::PositionHistory;
using rtype::ServerGame;
using namespace std::chrono_literals;
namespace tuning = rtype::tuning;

namespace {

// A game with a player at slot 0 and an enemy going down at 240 px/s (4 pixels a tick) at
// x = 700, after a second of play: the history holds its last 30 positions.
struct Scene {
    ServerGame game;
    Entity ship;
    Entity enemy;

    Scene() : ship{game.add_player(0)}, enemy{game.registry().create()} {
        auto& world = game.registry();
        world.emplace<Transform>(enemy, Vec2{700.0F, 100.0F});
        world.emplace<Velocity>(enemy, Vec2{0.0F, 240.0F});
        world.emplace<Collider>(enemy, Collider{.size = tuning::bydos_size,
                                                .layer = rtype::layer::enemy,
                                                .mask = rtype::mask::enemy});
        world.emplace<Health>(enemy, 1);
        for (std::uint32_t tick = 0; tick < tuning::tick_rate; ++tick) {
            step();
        }
    }

    void step() { game.tick(step_from_rate(tuning::tick_rate)); }

    // A missile of the player, still, where the enemy was @p ticks before the next tick.
    Entity missile_at(std::uint32_t ticks) {
        auto& world = game.registry();
        const Entity missile = rtype::spawn_player_missile(world, ship);
        world.get<Velocity>(missile).value = {};
        const std::optional<Vec2> seen = game.history().position(enemy, game.ticks() + 1 - ticks);
        REQUIRE(seen.has_value());
        world.get<Transform>(missile).position = seen.value_or(Vec2{});
        return missile;
    }

    bool killed() const {
        return std::ranges::any_of(game.damage().kills,
                                   [this](const auto& kill) { return kill.victim == enemy; });
    }
};

} // namespace

TEST_CASE("a player sees the enemies its round trip and interpolation delay ago", "[server][lag]") {
    // 200 ms of round trip and the client's 100 ms of interpolation: 18 ticks.
    CHECK(rtype::rewind_ticks(200ms, tuning::client_interpolation_delay) == 18);
    CHECK(rtype::rewind_ticks(0ms, 0ms) == 0);
    CHECK(rtype::rewind_ticks(7ms, 0ms) == 0);
    CHECK(rtype::rewind_ticks(9ms, 0ms) == 1);
    // Never further back than the history keeps.
    CHECK(rtype::rewind_ticks(1s, tuning::client_interpolation_delay) == PositionHistory::capacity);
}

TEST_CASE("at 200 ms of round trip a missile that hits on the player's screen hits on the server",
          "[server][lag]") {
    const std::uint32_t rewind = rtype::rewind_ticks(200ms, tuning::client_interpolation_delay);
    {
        // Compensated: the missile is where the player saw the enemy, 72 pixels above where
        // it is now; it hits.
        Scene scene;
        REQUIRE(scene.game.set_rewind(scene.ship, rewind));
        CHECK(scene.game.rewind(scene.ship) == rewind);
        const Entity missile = scene.missile_at(rewind);
        scene.step();
        CHECK(scene.killed());
        CHECK(std::ranges::find(scene.game.damage().consumed, missile) !=
              scene.game.damage().consumed.end());
    }
    {
        // Not compensated: the same shot misses the enemy, which moved on.
        Scene scene;
        const Entity missile = scene.missile_at(rewind);
        scene.step();
        CHECK(!scene.killed());
        CHECK(scene.game.registry().alive(missile));
        CHECK(scene.game.registry().alive(scene.enemy));
    }
}

TEST_CASE("lag compensation stays within its window and spares the dead", "[server][lag]") {
    {
        // A rewind of 60 ticks is held to 30: a shot at where the enemy was 40 ticks ago misses.
        Scene scene;
        REQUIRE(scene.game.set_rewind(scene.ship, 60));
        CHECK(scene.game.rewind(scene.ship) == PositionHistory::capacity);
        auto& world = scene.game.registry();
        const Entity missile = rtype::spawn_player_missile(world, scene.ship);
        world.get<Velocity>(missile).value = {};
        world.get<Transform>(missile).position =
            world.get<Transform>(scene.enemy).position - Vec2{0.0F, 4.0F * 40.0F};
        scene.step();
        CHECK(!scene.killed());
    }
    {
        // An enemy destroyed since is not hit where it was.
        Scene scene;
        REQUIRE(scene.game.set_rewind(scene.ship, 18));
        const Entity missile = scene.missile_at(18);
        scene.game.registry().destroy(scene.enemy);
        scene.step();
        CHECK(scene.game.registry().alive(missile));
    }
    {
        // A player out has no rewind; one who joins again starts without.
        Scene scene;
        REQUIRE(scene.game.set_rewind(scene.ship, 18));
        REQUIRE(scene.game.remove_player(scene.ship));
        CHECK(scene.game.rewind(scene.ship) == 0);
        CHECK(!scene.game.set_rewind(scene.ship, 18));
        const Entity again = scene.game.add_player(0);
        CHECK(scene.game.rewind(again) == 0);
    }
}
