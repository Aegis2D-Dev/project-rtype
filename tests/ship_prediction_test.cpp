#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/core/random.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/system.hpp>
#include <aegis2d/net/sequence.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <rtype/delta_snapshot.hpp>
#include <rtype/input.hpp>
#include <rtype/server_game.hpp>
#include <rtype/ship_movement.hpp>
#include <rtype/ship_prediction.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

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

TEST_CASE("without loss the reconciliation corrects nothing", "[client][reconciliation]") {
    // A server 150 ms away (5 ticks each way) applying each input at one tick, and sending
    // the ship's position, rounded as snapshots round it, every third tick.
    constexpr std::size_t latency = 5;
    rtype::ServerGame server;
    const aegis2d::ecs::Entity ship = server.add_player(0);
    struct Snapshot {
        Vec2 position;
        std::optional<std::uint16_t> input;
    };
    std::deque<std::pair<std::size_t, InputRecord>> to_server;
    std::deque<std::pair<std::size_t, Snapshot>> to_client;
    std::optional<std::uint16_t> applied;
    ShipPrediction prediction;
    std::size_t reconciliations = 0;

    // Still until the prediction starts, then every direction, the edges and back.
    const std::array<std::uint8_t, 5> pattern{held(Action::Right), held(Action::Up),
                                              held(Action::Left), held(Action::Down), 0};
    std::uint16_t sequence = 0;
    for (std::size_t now = 0; now < 1200; ++now) {
        const std::uint8_t actions =
            prediction.ship().has_value() ? pattern.at((now / 90) % pattern.size()) : 0;
        const InputRecord input{.sequence = sequence++, .actions = actions};
        prediction.apply(input, tick);
        to_server.emplace_back(now + latency, input);

        while (!to_server.empty() && to_server.front().first == now) {
            const InputRecord arrived = to_server.front().second;
            to_server.pop_front();
            REQUIRE(server.set_input(ship, PlayerInput{.held = arrived.actions}));
            applied = arrived.sequence;
        }
        server.tick(aegis2d::core::step_from_rate(rtype::tuning::tick_rate));
        if (now % 3 == 0) {
            const Vec2 at = server.registry().get<aegis2d::ecs::Transform>(ship).position;
            const rtype::EntityRecord sent =
                rtype::quantized(rtype::EntityRecord{.id = 1, .kind = 1, .x = at.x, .y = at.y});
            to_client.emplace_back(now + latency,
                                   Snapshot{.position = {sent.x, sent.y}, .input = applied});
        }
        while (!to_client.empty() && to_client.front().first == now) {
            const Snapshot received = to_client.front().second;
            to_client.pop_front();
            if (!prediction.ship().has_value()) {
                prediction.start(1, received.position);
                continue;
            }
            CHECK(prediction.reconcile(received.position, received.input, tick) <=
                  ShipPrediction::correction_threshold);
            ++reconciliations;
        }
    }
    CHECK(reconciliations > 350);
    CHECK(prediction.corrections() == 0);
    // About 10 inputs on their way: those of the round trip, and the snapshot's interval.
    CHECK(prediction.pending().size() <= 2 * latency + 3);
}

TEST_CASE("a server correction is replayed from where the server put the ship",
          "[client][reconciliation]") {
    ShipPrediction prediction;
    const Vec2 start{400.0F, 360.0F};
    prediction.start(7, start);
    for (std::uint16_t sequence = 1; sequence <= 6; ++sequence) {
        prediction.apply(InputRecord{.sequence = sequence, .actions = held(Action::Right)}, tick);
    }
    const Vec2 predicted = prediction.position().value_or(Vec2{});

    // The server applied inputs 1 to 3, but something pushed the ship 20 pixels down: the
    // ship goes there, and inputs 4 to 6 are replayed from it.
    Vec2 server = start;
    for (int step = 0; step < 3; ++step) {
        server = rtype::step_ship(server, PlayerInput{.held = held(Action::Right)}, tick).position;
    }
    server.y += 20.0F;
    const float moved = prediction.reconcile(server, std::uint16_t{3}, tick);
    Vec2 expected = server;
    for (int step = 0; step < 3; ++step) {
        expected =
            rtype::step_ship(expected, PlayerInput{.held = held(Action::Right)}, tick).position;
    }
    CHECK(prediction.position() == std::optional{expected});
    CHECK(prediction.position().value_or(Vec2{}).x == Catch::Approx(predicted.x));
    CHECK(moved == Catch::Approx(20.0F));
    CHECK(prediction.last_correction() == moved);
    CHECK(prediction.corrections() == 1);
    REQUIRE(prediction.pending().size() == 3);
    CHECK(prediction.pending().front().sequence == 4);

    // Before any input applied, every input kept is replayed; the server agreeing is no
    // correction.
    ShipPrediction fresh;
    fresh.start(7, start);
    fresh.apply(InputRecord{.sequence = 0, .actions = held(Action::Up)}, tick);
    const Vec2 up = fresh.position().value_or(Vec2{});
    CHECK(fresh.reconcile(start, std::nullopt, tick) == 0.0F);
    CHECK(fresh.position() == std::optional{up});
    CHECK(fresh.corrections() == 0);
    CHECK(fresh.pending().size() == 1);

    // Nothing to correct before start().
    ShipPrediction idle;
    CHECK(idle.reconcile(start, std::uint16_t{1}, tick) == 0.0F);
    CHECK(!idle.position().has_value());
}

TEST_CASE("a small correction is drawn over a few frames, a large one at once",
          "[client][smoothing]") {
    ShipPrediction prediction;
    const Vec2 start{400.0F, 360.0F};
    prediction.start(7, start);
    CHECK(prediction.drawn() == std::optional{start});

    // The server had the ship 10 pixels lower: the ship is there, the drawn one not yet.
    prediction.reconcile(start + Vec2{0.0F, 10.0F}, std::nullopt, tick);
    CHECK(prediction.position() == std::optional{start + Vec2{0.0F, 10.0F}});
    CHECK(prediction.drawn() == std::optional{start});
    // 2 pixels a frame at 60 frames per second: there in 5 frames.
    float previous = start.y;
    for (int frame = 0; frame < 5; ++frame) {
        prediction.smooth(1.0F / 60.0F);
        const float y = prediction.drawn().value_or(Vec2{}).y;
        CHECK(y - previous == Catch::Approx(2.0F).margin(0.001F));
        previous = y;
    }
    prediction.smooth(1.0F / 60.0F);
    CHECK(prediction.drawn() == prediction.position());

    // From snap_distance on, the ship is drawn where it is at once.
    prediction.reconcile(start + Vec2{ShipPrediction::snap_distance, 10.0F}, std::nullopt, tick);
    CHECK(prediction.drawn() == prediction.position());

    // A start leaves no gap.
    prediction.reconcile(prediction.position().value_or(start) + Vec2{0.0F, 4.0F}, std::nullopt,
                         tick);
    CHECK(prediction.drawn() != prediction.position());
    prediction.start(7, start);
    CHECK(prediction.drawn() == std::optional{start});
    prediction.stop();
    CHECK(!prediction.drawn().has_value());
}

TEST_CASE("at 5 % loss the ship never jumps more than a few pixels per frame",
          "[client][smoothing]") {
    // A server 150 ms away (5 ticks each way): each Input datagram carries 4 inputs, the
    // server sets those it has not applied before its next tick and holds the last one;
    // snapshots every third tick. 5 % of the datagrams are lost each way.
    constexpr std::size_t latency = 5;
    rtype::ServerGame server;
    const aegis2d::ecs::Entity ship = server.add_player(0);
    struct Snapshot {
        std::size_t tick;
        Vec2 position;
        std::optional<std::uint16_t> input;
    };
    std::deque<std::pair<std::size_t, std::vector<InputRecord>>> to_server;
    std::deque<std::pair<std::size_t, Snapshot>> to_client;
    std::optional<std::uint16_t> applied;
    std::vector<InputRecord> window;
    ShipPrediction prediction;
    aegis2d::core::Random random{294};
    const auto lost = [&random] { return random.below(100) < 5; };
    const std::array<std::uint8_t, 6> directions{
        held(Action::Right), held(Action::Up), held(Action::Left), held(Action::Down), 0,
        held(Action::Right)};
    std::uint8_t actions = 0;
    std::optional<Vec2> drawn;
    float largest_jump = 0.0F;
    float largest_correction = 0.0F;
    std::uint16_t sequence = 0;
    for (std::size_t now = 0; now < 3600; ++now) {
        // A new direction every 8 ticks, once the prediction runs.
        if (prediction.ship().has_value() && now % 8 == 0) {
            actions = directions.at(random.below(static_cast<std::uint32_t>(directions.size())));
        }
        const InputRecord input{.sequence = sequence++, .actions = actions};
        prediction.apply(input, tick);
        if (window.size() == rtype::max_inputs_per_datagram) {
            window.erase(window.begin());
        }
        window.push_back(input);
        if (!lost()) {
            to_server.emplace_back(now + latency, window);
        }

        while (!to_server.empty() && to_server.front().first == now) {
            for (const InputRecord& arrived : to_server.front().second) {
                if (!applied || aegis2d::net::sequence_greater_than(arrived.sequence, *applied)) {
                    REQUIRE(server.set_input(ship, PlayerInput{.held = arrived.actions}));
                    applied = arrived.sequence;
                }
            }
            to_server.pop_front();
        }
        server.tick(aegis2d::core::step_from_rate(rtype::tuning::tick_rate));
        if (now % 3 == 0 && !lost()) {
            const Vec2 at = server.registry().get<aegis2d::ecs::Transform>(ship).position;
            const rtype::EntityRecord sent =
                rtype::quantized(rtype::EntityRecord{.id = 1, .kind = 1, .x = at.x, .y = at.y});
            to_client.emplace_back(
                now + latency,
                Snapshot{.tick = now, .position = {sent.x, sent.y}, .input = applied});
        }
        while (!to_client.empty() && to_client.front().first == now) {
            const Snapshot received = to_client.front().second;
            to_client.pop_front();
            if (!prediction.ship().has_value()) {
                prediction.start(1, received.position);
            } else {
                largest_correction =
                    std::max(largest_correction,
                             prediction.reconcile(received.position, received.input, tick));
            }
        }

        // One frame per tick, at 60 frames per second.
        prediction.smooth(tick);
        if (const std::optional<Vec2> at = prediction.drawn()) {
            if (drawn) {
                const Vec2 moved = *at - *drawn;
                largest_jump = std::max(largest_jump, std::hypot(moved.x, moved.y));
            }
            drawn = at;
        }
    }
    CAPTURE(prediction.corrections(), largest_correction, largest_jump);
    // The loss makes the server apply some inputs for more or fewer ticks than the client.
    CHECK(prediction.corrections() > 10);
    CHECK(largest_correction > 5.0F);
    // The ship's own move (5 pixels a tick) and 2 pixels of correction at most.
    CHECK(largest_jump <= (rtype::tuning::player_speed * tick) + 2.0F + 0.01F);
}
