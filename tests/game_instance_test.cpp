#include <aegis2d/core/clock.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/waves.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/entities.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/server_game.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

using aegis2d::core::step_from_rate;
using aegis2d::ecs::Entity;
using aegis2d::ecs::Transform;
using aegis2d::ecs::WaveSchedule;
using rtype::GameInstance;
namespace tuning = rtype::tuning;

namespace {

GameInstance empty_instance() {
    return GameInstance{std::nullopt, WaveSchedule{}};
}

// Shoots a player until it has no life left.
void knock_out(GameInstance& instance, Entity ship) {
    for (std::uint32_t life = 0; life < tuning::player_lives; ++life) {
        const auto position = instance.game().registry().get<Transform>(ship).position;
        rtype::spawn_enemy_missile(instance.game().registry(), position);
        instance.game().tick(step_from_rate(tuning::tick_rate));
    }
}

} // namespace

TEST_CASE("each player gets the ship of its number", "[server][game]") {
    GameInstance instance = empty_instance();
    CHECK(instance.players() == 0);
    CHECK(instance.games() == 1);

    const Entity two = instance.join(2);
    const Entity zero = instance.join(0);
    CHECK(instance.game().slot_of(two) == 2);
    CHECK(instance.game().slot_of(zero) == 0);
    CHECK(instance.ship(2) == two);
    CHECK(instance.ship(1) == std::nullopt);
    CHECK(instance.players() == 2);
    CHECK(instance.game().players() == 2);

    CHECK_THROWS_AS(instance.join(2), std::logic_error);
    CHECK_THROWS_AS(instance.join(tuning::max_players), std::out_of_range);
    CHECK(instance.join(1) != Entity{});
    CHECK(instance.join(3) != Entity{});
    CHECK(instance.players() == tuning::max_players);
}

TEST_CASE("a player who leaves frees its number for the next one", "[server][game]") {
    GameInstance instance = empty_instance();
    const Entity zero = instance.join(0);
    const Entity one = instance.join(1);

    CHECK(instance.leave(1));
    CHECK(!instance.leave(1));
    CHECK(!instance.leave(tuning::max_players));
    CHECK(!instance.game().registry().alive(one));
    CHECK(instance.ship(1) == std::nullopt);
    CHECK(instance.players() == 1);
    CHECK(instance.games() == 1);
    CHECK(instance.game().registry().alive(zero));

    const Entity again = instance.join(1);
    CHECK(instance.game().slot_of(again) == 1);
    CHECK(instance.players() == 2);
}

TEST_CASE("a player out of lives keeps its number until it leaves", "[server][game]") {
    GameInstance instance = empty_instance();
    const Entity zero = instance.join(0);
    static_cast<void>(instance.join(1));
    knock_out(instance, zero);

    CHECK(!instance.game().registry().alive(zero));
    CHECK(instance.ship(0) == std::nullopt);
    CHECK(instance.players() == 2);
    CHECK(instance.game().players() == 1);
    CHECK_THROWS_AS(instance.join(0), std::logic_error);
    CHECK(instance.leave(0));
    CHECK(instance.players() == 1);
}

TEST_CASE("the last player leaving starts a fresh game", "[server][game]") {
    GameInstance instance = empty_instance();
    const Entity ship = instance.join(0);
    instance.game().tick(step_from_rate(tuning::tick_rate));
    CHECK(instance.game().ticks() == 1);
    knock_out(instance, ship);
    CHECK(instance.game().game_over());

    CHECK(instance.leave(0));
    CHECK(instance.games() == 2);
    CHECK(instance.players() == 0);
    CHECK(instance.game().ticks() == 0);
    CHECK(!instance.game().game_over());
    CHECK(instance.game().slot_of(instance.join(0)) == 0);
}
