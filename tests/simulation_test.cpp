#include <aegis2d/ecs/waves.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/simulation.hpp>
#include <rtype/tuning.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

using aegis2d::ecs::WaveSchedule;
using rtype::GameInstance;
using rtype::Simulation;

namespace {

// Waits until the condition holds, for at most two seconds.
template <typename Condition>
bool wait_until(Condition condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    return true;
}

} // namespace

TEST_CASE("the simulation ticks at 60 Hz with no player", "[server][simulation]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    Simulation simulation{instance};
    CHECK(simulation.rate() == rtype::tuning::tick_rate);

    // Measured over one second after the first tick, so the thread's start does not count.
    REQUIRE(wait_until([&] { return simulation.ticks() > 0; }));
    const std::uint64_t first = simulation.ticks();
    const auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds{1});
    const std::uint64_t ticks = simulation.ticks() - first;
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    simulation.stop();

    // Loaded CI runners sleep late: a wide margin, but far from 30 or 120 Hz.
    const double rate = static_cast<double>(ticks) / elapsed.count();
    CAPTURE(ticks, elapsed.count(), rate);
    CHECK(rate > 50.0);
    CHECK(rate < 70.0);

    // Stopped: no more ticks, and the game is the instance's to read again.
    const std::uint64_t stopped = simulation.ticks();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    CHECK(simulation.ticks() == stopped);
    CHECK(instance.game().ticks() == stopped);
}

TEST_CASE("commands from other threads run on the simulation thread, in order",
          "[server][simulation]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    Simulation simulation{instance};
    std::vector<int> order;
    std::atomic<bool> done{false};
    std::thread::id where;

    std::thread network{[&] {
        simulation.post([](GameInstance& game) { static_cast<void>(game.join(2)); });
        simulation.post([&](GameInstance& /*game*/) { order.push_back(1); });
        simulation.post([](GameInstance& /*game*/) { throw std::runtime_error{"bad command"}; });
        simulation.post([&](GameInstance& game) {
            order.push_back(2);
            where = std::this_thread::get_id();
            static_cast<void>(game.join(0));
            done.store(true);
        });
    }};
    network.join();
    REQUIRE(wait_until([&] { return done.load(); }));
    simulation.stop();

    // A command that throws is logged; the others run, and so do the ticks.
    CHECK(order == std::vector<int>{1, 2});
    CHECK(where != std::this_thread::get_id());
    CHECK(instance.players() == 2);
    CHECK(instance.ship(2).has_value());
    CHECK(instance.ship(0).has_value());
}

TEST_CASE("the simulation stops at once, even before its first tick", "[server][simulation]") {
    GameInstance instance{std::nullopt, WaveSchedule{}};
    for (int attempt = 0; attempt < 20; ++attempt) {
        Simulation simulation{instance};
        simulation.stop();
        simulation.stop();
    }
    {
        // Commands posted after the stop never run.
        Simulation simulation{instance};
        simulation.stop();
        bool ran = false;
        simulation.post([&ran](GameInstance& /*game*/) { ran = true; });
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        CHECK(!ran);
    }
    CHECK(instance.players() == 0);
}
