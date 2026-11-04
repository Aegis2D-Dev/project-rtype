#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/system.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/input.hpp>
#include <rtype/ship_movement.hpp>
#include <rtype/ship_prediction.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

using aegis2d::Vec2;
using aegis2d::ecs::Action;
using aegis2d::ecs::PlayerInput;
using rtype::InputRecord;
using rtype::ShipPrediction;

namespace {

const float tick =
    aegis2d::ecs::Tick{.step = aegis2d::core::step_from_rate(rtype::tuning::tick_rate), .index = 0}
        .delta();

std::uint8_t held(Action action) {
    PlayerInput input;
    input.set(action, true);
    return input.held;
}

} // namespace

TEST_CASE("the predicted ship moves at once by each input", "[client][prediction]") {
    ShipPrediction prediction;
    // Nothing to move before the server says where the ship is.
    prediction.apply(InputRecord{.sequence = 0, .actions = held(Action::Right)}, tick);
    CHECK(!prediction.position().has_value());
    CHECK(prediction.pending().empty());

    const Vec2 start{100.0F, 360.0F};
    prediction.start(7, start);
    CHECK(prediction.ship() == std::optional<std::uint32_t>{7});
    CHECK(prediction.position() == std::optional{start});

    // Each input is the server's step, and is kept.
    Vec2 expected = start;
    for (std::uint16_t sequence = 1; sequence <= 10; ++sequence) {
        const std::uint8_t actions = held(sequence <= 5 ? Action::Right : Action::Up);
        prediction.apply(InputRecord{.sequence = sequence, .actions = actions}, tick);
        expected = rtype::step_ship(expected, PlayerInput{.held = actions}, tick).position;
        CHECK(prediction.position() == std::optional{expected});
    }
    CHECK(prediction.pending().size() == 10);
    CHECK(prediction.pending().front().sequence == 1);

    // The play area holds it, as on the server.
    for (std::uint16_t sequence = 11; sequence < 100; ++sequence) {
        prediction.apply(InputRecord{.sequence = sequence, .actions = held(Action::Up)}, tick);
    }
    CHECK(prediction.position().value_or(Vec2{}).y == rtype::tuning::player_size.y / 2.0F);
}

TEST_CASE("acknowledged inputs are dropped, the others kept", "[client][prediction]") {
    ShipPrediction prediction;
    prediction.start(7, Vec2{640.0F, 360.0F});
    // Through the wrap of the sequences.
    for (std::uint16_t offset = 0; offset < 6; ++offset) {
        prediction.apply(
            InputRecord{.sequence = static_cast<std::uint16_t>(65533U + offset), .actions = 0},
            tick);
    }
    prediction.acknowledge(65532); // older than every input: nothing dropped
    CHECK(prediction.pending().size() == 6);
    prediction.acknowledge(65535);
    REQUIRE(prediction.pending().size() == 3);
    CHECK(prediction.pending().front().sequence == 0);
    prediction.acknowledge(1);
    CHECK(prediction.pending().size() == 1);
    prediction.acknowledge(0); // late: changes nothing
    CHECK(prediction.pending().size() == 1);
    prediction.acknowledge(2);
    CHECK(prediction.pending().empty());
}

TEST_CASE("the prediction keeps two seconds of inputs, and restarts clean",
          "[client][prediction]") {
    ShipPrediction prediction;
    prediction.start(7, Vec2{640.0F, 360.0F});
    for (std::uint16_t sequence = 0; sequence < 200; ++sequence) {
        prediction.apply(InputRecord{.sequence = sequence, .actions = 0}, tick);
    }
    REQUIRE(prediction.pending().size() == ShipPrediction::max_pending);
    CHECK(prediction.pending().front().sequence == 200 - ShipPrediction::max_pending);

    // Another start (a respawn, another ship) forgets the inputs; stop() forgets the ship.
    prediction.start(9, Vec2{100.0F, 144.0F});
    CHECK(prediction.pending().empty());
    CHECK(prediction.ship() == std::optional<std::uint32_t>{9});
    prediction.stop();
    CHECK(!prediction.ship().has_value());
    CHECK(!prediction.position().has_value());
    prediction.apply(InputRecord{.sequence = 200, .actions = held(Action::Left)}, tick);
    CHECK(prediction.pending().empty());
}
