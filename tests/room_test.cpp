#include <aegis2d/ecs/waves.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/room.hpp>
#include <rtype/tuning.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>

TEST_CASE("a room runs its own game on its own thread", "[server][room]") {
    std::atomic<std::uint64_t> ticks{0};
    std::atomic<bool> other_thread{false};
    const std::thread::id caller = std::this_thread::get_id();
    std::atomic<bool> joined{false};
    {
        rtype::Room room{3, std::nullopt, aegis2d::ecs::WaveSchedule{}, rtype::tuning::tick_rate,
                         [&](rtype::Room& /*room*/, rtype::GameInstance& instance) {
                             ticks = instance.game().ticks();
                             other_thread = std::this_thread::get_id() != caller;
                         }};
        // Commands posted to the room reach its game before its next tick.
        room.simulation().post([&](rtype::GameInstance& instance) {
            static_cast<void>(instance.join(2));
            joined = true;
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while ((ticks < 6 || !joined) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        room.simulation().stop();
        CHECK(room.id() == 3);
        CHECK(ticks >= 6);
        CHECK(other_thread);
        CHECK(joined);
        CHECK(room.instance().players() == 1);
        CHECK(room.instance().ship(2).has_value());
        CHECK(room.instance().game().ticks() == ticks);
        CHECK(!room.names().at(2).has_value());
    }
    // Destroyed: the thread stopped, nothing ticks any more.
    const std::uint64_t last = ticks;
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    CHECK(ticks == last);
}
