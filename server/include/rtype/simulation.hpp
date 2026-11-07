/// @file
/// @brief Thread running the server's game at a fixed rate.
#pragma once
#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/loop.hpp>
#include <aegis2d/core/profiler.hpp>

#include <rtype/game_instance.hpp>
#include <rtype/tuning.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace rtype {

/// @brief Runs a GameInstance on its own thread, tuning::tick_rate times per second.
/// @details The thread starts in the constructor and ticks the game at a fixed rate, with
/// or without players, using the engine's `MainLoop`: it sleeps until each step is due and,
/// after a stall, catches up at most `FixedTimestep::default_max_frame` of steps. Every
/// report_period it logs how many ticks ran and the rate measured.
///
/// The game belongs to the thread: other threads (the network) never touch it, they post()
/// commands that run on the thread before the next tick. An exception thrown by a command
/// or by a tick is logged and the thread goes on.
class Simulation {
public:
    /// @brief A change to make to the game, run on the simulation thread.
    using Command = std::function<void(GameInstance& instance)>;

    /// @brief Called on the simulation thread after each tick, to read the game.
    using TickHandler = std::function<void(GameInstance& instance)>;

    /// @brief How often the measured rate is logged.
    static constexpr std::chrono::seconds report_period{10};

    /// @brief Starts the thread.
    /// @param instance Game to run; it must outlive this object and is only used by the
    /// thread from now on.
    /// @param rate Ticks per second, tuning::tick_rate by default.
    /// @param after_tick Called after each tick (snapshots, events); may be empty.
    /// @param trace `true` to time each tick, its systems and what follows it, for profiler().
    explicit Simulation(GameInstance& instance, std::uint32_t rate = tuning::tick_rate,
                        TickHandler after_tick = {}, bool trace = false);

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;
    Simulation(Simulation&&) = delete;
    Simulation& operator=(Simulation&&) = delete;
    /// @brief Stops the thread and waits for it.
    ~Simulation();

    /// @brief Queues a command for the simulation thread. Thread-safe.
    /// @details Commands run in the order they were posted, all of them before the next
    /// tick. Commands posted after stop() never run.
    /// @param command Change to make to the game.
    void post(Command command);

    /// @brief Stops the thread after its current tick and waits for it. Thread-safe, but not
    /// from a command; calling it again does nothing.
    void stop();

    /// @brief Number of ticks run. Thread-safe.
    /// @return The count since the start.
    [[nodiscard]] std::uint64_t ticks() const noexcept;

    /// @brief The timings of the ticks, recorded with `trace`: a frame per tick, the `tick`
    /// and its systems, then `after_tick` (the snapshots and events).
    /// @details Used by the thread: read it once stop() returned.
    /// @return The profiler; off without `trace`.
    [[nodiscard]] const aegis2d::core::Profiler& profiler() const noexcept;

    /// @brief Ticks per second the thread aims at.
    /// @return The rate given to the constructor.
    [[nodiscard]] std::uint32_t rate() const noexcept;

private:
    void run();
    void step(aegis2d::core::Duration step);
    void run_commands();

    GameInstance& instance_;
    std::uint32_t rate_;
    TickHandler after_tick_;
    aegis2d::core::MainLoop loop_;
    std::mutex mutex_;
    std::vector<Command> commands_;
    std::atomic<std::uint64_t> ticks_{0};
    std::atomic<bool> stopping_{false};
    // Used by the thread only.
    std::chrono::steady_clock::time_point report_since_;
    std::uint64_t report_ticks_ = 0;
    aegis2d::core::Profiler profiler_;
    // Last, so the thread starts once every other member is ready.
    std::thread thread_;
};

} // namespace rtype
