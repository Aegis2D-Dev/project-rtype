#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <rtype/entities.hpp>
#include <rtype/server_game.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

using aegis2d::Vec2;
using aegis2d::core::step_from_rate;
using aegis2d::ecs::Action;
using aegis2d::ecs::Entity;
using aegis2d::ecs::PlayerInput;
using aegis2d::ecs::Score;
using aegis2d::ecs::Transform;
using Catch::Approx;
using rtype::ServerGame;
namespace tuning = rtype::tuning;

namespace {

void run(ServerGame& game, int ticks) {
    for (int tick = 0; tick < ticks; ++tick) {
        game.tick(step_from_rate(tuning::tick_rate));
    }
}

PlayerInput holding(Action action) {
    PlayerInput input;
    input.set(action, true);
    return input;
}

Vec2 position_of(const ServerGame& game, Entity entity) {
    return game.registry().get<Transform>(entity).position;
}

void place(ServerGame& game, Entity entity, Vec2 position) {
    game.registry().get<Transform>(entity).position = position;
}

} // namespace

TEST_CASE("the server registers its systems in tick order", "[rtype][server]") {
    const ServerGame game;
    CHECK(game.systems().names() ==
          std::vector<std::string_view>{"steering", "movement", "play_area", "collision", "damage",
                                        "score", "lives", "cleanup"});
    CHECK(game.ticks() == 0);
    CHECK(game.players() == 0);
    CHECK_FALSE(game.game_over());
}

TEST_CASE("up to four players join in their own slots", "[rtype][server]") {
    ServerGame game;
    std::vector<Entity> ships;
    for (std::size_t slot = 0; slot < tuning::max_players; ++slot) {
        ships.push_back(game.add_player());
        CHECK(game.slot_of(ships.back()) == slot);
        CHECK(position_of(game, ships.back()) == rtype::player_spawn(slot));
    }
    CHECK(game.players() == 4);
    CHECK_THROWS_AS(game.add_player(), std::length_error);
    CHECK_FALSE(game.slot_of(game.registry().create()));
}

TEST_CASE("a player ship moves at a fixed speed per second", "[rtype][server][131]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Vec2 start = position_of(game, ship);
    CHECK(game.set_input(ship, holding(Action::Down)));

    run(game, static_cast<int>(tuning::tick_rate / 2));
    CHECK(position_of(game, ship).y == Approx(start.y + (tuning::player_speed / 2.0F)));
    CHECK(position_of(game, ship).x == Approx(start.x));
    CHECK(game.ticks() == tuning::tick_rate / 2);
}

TEST_CASE("holding an arrow stops at the edge", "[rtype][server][132]") {
    ServerGame game;
    const Entity ship = game.add_player();

    game.set_input(ship, holding(Action::Right));
    run(game, 10 * static_cast<int>(tuning::tick_rate));
    CHECK(position_of(game, ship).x == Approx(tuning::area_width - (tuning::player_size.x / 2.0F)));

    game.set_input(ship, holding(Action::Up));
    run(game, 10 * static_cast<int>(tuning::tick_rate));
    CHECK(position_of(game, ship).y == Approx(tuning::player_size.y / 2.0F));

    game.set_input(ship, holding(Action::Left));
    run(game, 10 * static_cast<int>(tuning::tick_rate));
    CHECK(position_of(game, ship).x == Approx(tuning::player_size.x / 2.0F));
}

TEST_CASE("a missile kills a Bydos and credits its shooter", "[rtype][server][140][141]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Entity bydos = rtype::spawn_bydos_slave(game.registry(), Vec2{400.0F, 0.0F});
    place(game, bydos, Vec2{400.0F, position_of(game, ship).y});
    const Entity missile = rtype::spawn_player_missile(game.registry(), ship);

    bool killed = false;
    for (int tick = 0; tick < 60 && !killed; ++tick) {
        run(game, 1);
        killed = !game.registry().alive(bydos);
    }
    REQUIRE(killed);
    CHECK_FALSE(game.registry().alive(missile));
    CHECK(game.registry().get<Score>(ship).points == tuning::bydos_score);
    REQUIRE(game.awards().size() == 1);
    CHECK(game.awards().front().player == ship);
    CHECK(game.damage().kills.size() == 1);
}

TEST_CASE("each player's score increases on their own kills", "[rtype][server][141]") {
    ServerGame game;
    const Entity one = game.add_player();
    const Entity two = game.add_player();
    const auto bydos_in_front_of = [&game](Entity ship, float x) {
        const Entity bydos = rtype::spawn_bydos_slave(game.registry(), Vec2{});
        place(game, bydos, Vec2{x, position_of(game, ship).y});
        return bydos;
    };
    bydos_in_front_of(one, 500.0F);
    bydos_in_front_of(two, 500.0F);
    bydos_in_front_of(two, 900.0F);
    rtype::spawn_player_missile(game.registry(), one);
    rtype::spawn_player_missile(game.registry(), two);
    run(game, 30);
    rtype::spawn_player_missile(game.registry(), two);
    run(game, 60);

    CHECK(game.registry().get<Score>(one).points == tuning::bydos_score);
    CHECK(game.registry().get<Score>(two).points == 2 * tuning::bydos_score);
}

TEST_CASE("enemies and missiles of the same side pass through each other", "[rtype][server][139]") {
    ServerGame game;
    const Entity one = game.add_player();
    const Entity two = game.add_player();
    place(game, two, position_of(game, one));
    const Entity first = rtype::spawn_bydos_slave(game.registry(), Vec2{900.0F, 300.0F});
    const Entity second = rtype::spawn_bydos_slave(game.registry(), Vec2{900.0F, 300.0F});
    const Entity bullet = rtype::spawn_enemy_missile(game.registry(), Vec2{905.0F, 300.0F});
    const Entity missile = rtype::spawn_player_missile(game.registry(), one);

    run(game, 1);
    CHECK(game.registry().alive(one));
    CHECK(game.registry().alive(two));
    CHECK(game.registry().alive(first));
    CHECK(game.registry().alive(second));
    CHECK(game.registry().alive(bullet));
    CHECK(game.registry().alive(missile));
    CHECK(game.damage().hits.empty());
}

TEST_CASE("a hit player respawns until the game is over", "[rtype][server][142]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Vec2 spawn = rtype::player_spawn(0);
    game.set_input(ship, holding(Action::Right));

    for (std::uint32_t life = tuning::player_lives; life > 0; --life) {
        REQUIRE(game.players() == 1);
        run(game, 20);
        rtype::spawn_enemy_missile(game.registry(), position_of(game, ship));
        run(game, 1);
        REQUIRE(game.lives_lost().size() == 1);
        CHECK(game.lives_lost().front().lives_left == life - 1);
        if (life > 1) {
            CHECK(position_of(game, ship) == spawn);
        }
    }
    CHECK_FALSE(game.registry().alive(ship));
    CHECK(game.players() == 0);
    CHECK(game.game_over());
    CHECK_FALSE(game.set_input(ship, holding(Action::Left)));
}

TEST_CASE("the game is over only when every player is out", "[rtype][server][142]") {
    ServerGame game;
    const Entity one = game.add_player();
    const Entity two = game.add_player();
    const auto shoot = [&game](Entity ship, int times) {
        for (int shot = 0; shot < times; ++shot) {
            rtype::spawn_enemy_missile(game.registry(), position_of(game, ship));
            run(game, 1);
        }
    };

    shoot(one, static_cast<int>(tuning::player_lives));
    CHECK_FALSE(game.registry().alive(one));
    CHECK_FALSE(game.game_over());
    CHECK(game.players() == 1);

    const Entity back = game.add_player();
    CHECK(game.slot_of(back) == 0);
    shoot(two, static_cast<int>(tuning::player_lives));
    CHECK_FALSE(game.game_over());
    shoot(back, static_cast<int>(tuning::player_lives));
    CHECK(game.game_over());
}

TEST_CASE("ramming a Bydos costs a life and kills it", "[rtype][server][140][142]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Entity bydos = rtype::spawn_bydos_slave(game.registry(), Vec2{});
    place(game, bydos, position_of(game, ship));

    run(game, 1);
    CHECK_FALSE(game.registry().alive(bydos));
    REQUIRE(game.lives_lost().size() == 1);
    CHECK(game.lives_lost().front().lives_left == tuning::player_lives - 1);
    CHECK(game.registry().get<Score>(ship).points == tuning::bydos_score);
}
