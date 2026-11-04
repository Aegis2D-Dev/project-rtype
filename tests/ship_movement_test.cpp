#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/system.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <rtype/server_game.hpp>
#include <rtype/ship_movement.hpp>
#include <rtype/tuning.hpp>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <vector>

using aegis2d::Vec2;
using aegis2d::core::step_from_rate;
using aegis2d::ecs::Action;
using aegis2d::ecs::PlayerInput;
using Catch::Approx;
using rtype::ServerGame;
using rtype::step_ship;
namespace tuning = rtype::tuning;

namespace {

PlayerInput holding(std::initializer_list<Action> actions) {
    PlayerInput input;
    for (const Action action : actions) {
        input.set(action, true);
    }
    return input;
}

// One tick at the server's rate, in seconds.
const float tick =
    aegis2d::ecs::Tick{.step = step_from_rate(tuning::tick_rate), .index = 0}.delta();

} // namespace

TEST_CASE("the ship step moves at the player speed in every direction", "[game][movement]") {
    const Vec2 center{640.0F, 360.0F};
    const rtype::ShipMotion right = step_ship(center, holding({Action::Right}), tick);
    CHECK(right.velocity == Vec2{tuning::player_speed, 0.0F});
    CHECK(right.position.x == Approx(center.x + (tuning::player_speed * tick)));
    CHECK(right.position.y == center.y);

    const rtype::ShipMotion up = step_ship(center, holding({Action::Up}), tick);
    CHECK(up.velocity == Vec2{0.0F, -tuning::player_speed});
    CHECK(up.position.y < center.y);

    // Diagonals are as fast as straight lines; opposite directions cancel out.
    const rtype::ShipMotion diagonal =
        step_ship(center, holding({Action::Down, Action::Left}), tick);
    CHECK(std::hypot(diagonal.velocity.x, diagonal.velocity.y) == Approx(tuning::player_speed));
    CHECK(diagonal.velocity.x < 0.0F);
    CHECK(diagonal.velocity.y > 0.0F);
    CHECK(step_ship(center, holding({Action::Left, Action::Right}), tick).position == center);

    // Nothing held, or only fire: the ship stays where it is.
    CHECK(step_ship(center, PlayerInput{}, tick) ==
          rtype::ShipMotion{.position = center, .velocity = {}});
    CHECK(step_ship(center, holding({Action::Fire}), tick).position == center);
}

TEST_CASE("the ship step keeps the ship inside the play area", "[game][movement]") {
    const Vec2 half = tuning::player_size * 0.5F;
    // Held against the left edge: the ship stops there, and leaves it at once.
    Vec2 position{half.x + 1.0F, 360.0F};
    for (int index = 0; index < 10; ++index) {
        position = step_ship(position, holding({Action::Left}), tick).position;
    }
    CHECK(position.x == half.x);
    CHECK(step_ship(position, holding({Action::Right}), tick).position.x > half.x);

    // Every corner of the area.
    const Vec2 top_left =
        step_ship(Vec2{0.0F, 0.0F}, holding({Action::Up, Action::Left}), tick).position;
    CHECK(top_left == half);
    const Vec2 bottom_right = step_ship(Vec2{tuning::area_width, tuning::area_height},
                                        holding({Action::Down, Action::Right}), tick)
                                  .position;
    CHECK(bottom_right == Vec2{tuning::area_width - half.x, tuning::area_height - half.y});

    // A ship far outside is brought back, whatever it holds.
    CHECK(rtype::keep_ship_in_area(Vec2{-500.0F, 5000.0F}) ==
          Vec2{half.x, tuning::area_height - half.y});
}

TEST_CASE("the server moves a ship tick by tick as the ship step says", "[game][movement]") {
    // No seed and no waves: the ship is alone and never dies.
    ServerGame game;
    const auto ship = game.add_player();
    auto& world = game.registry();
    Vec2 predicted = world.get<aegis2d::ecs::Transform>(ship).position;

    // Every direction, the diagonals, against the edges and away from them, with fire.
    const std::vector<PlayerInput> steps{
        holding({Action::Right}), holding({Action::Up, Action::Right}),
        holding({Action::Up}),    holding({Action::Up, Action::Left}),
        holding({Action::Left}),  holding({Action::Down, Action::Left}),
        holding({Action::Down}),  holding({Action::Down, Action::Right}),
        holding({Action::Fire}),  PlayerInput{}};
    for (std::size_t round = 0; round < 2; ++round) {
        for (const PlayerInput input : steps) {
            // Long enough to reach an edge of the area.
            for (int index = 0; index < 150; ++index) {
                REQUIRE(game.set_input(ship, input));
                game.tick(step_from_rate(tuning::tick_rate));
                const rtype::ShipMotion expected = step_ship(predicted, input, tick);
                predicted = expected.position;
                REQUIRE(world.get<aegis2d::ecs::Transform>(ship).position == expected.position);
                REQUIRE(world.get<aegis2d::ecs::Velocity>(ship).value == expected.velocity);
            }
        }
    }
}
