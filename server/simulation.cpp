#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/log.hpp>
#include <aegis2d/core/loop.hpp>

#include <rtype/game_instance.hpp>
#include <rtype/simulation.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <mutex>
#include <utility>
#include <vector>

namespace rtype {

namespace core = aegis2d::core;

Simulation::Simulation(GameInstance& instance, std::uint32_t rate, TickHandler after_tick)
    : instance_{instance}, rate_{rate}, after_tick_{std::move(after_tick)},
      loop_{core::FixedTimestep{core::step_from_rate(rate)}}, thread_{[this] { run(); }} {}

Simulation::~Simulation() {
    stop();
}

void Simulation::run() {
    loop_.set_update([this](core::Duration step_duration) { step(step_duration); });
    report_since_ = std::chrono::steady_clock::now();
    // MainLoop ignores a stop() made before run(): the flag catches a stop() that came first.
    if (!stopping_.load(std::memory_order_acquire)) {
        loop_.run();
    }
}

void Simulation::step(core::Duration step_duration) {
    if (stopping_.load(std::memory_order_acquire)) {
        loop_.stop();
        return;
    }
    run_commands();
    try {
        instance_.game().tick(step_duration);
        if (after_tick_) {
            after_tick_(instance_);
        }
    } catch (const std::exception& error) {
        core::log_error("simulation: a tick threw: {}", error.what());
    }
    const std::uint64_t ticks = ticks_.fetch_add(1, std::memory_order_relaxed) + 1;

    // The measured rate, logged every report_period of real time.
    const auto now = std::chrono::steady_clock::now();
    if (now - report_since_ >= report_period) {
        const std::chrono::duration<double> elapsed = now - report_since_;
        const std::uint64_t count = ticks - report_ticks_;
        core::log_info("simulation: {} ticks in {:.1f} s ({:.1f} Hz)", count, elapsed.count(),
                       static_cast<double>(count) / elapsed.count());
        report_since_ = now;
        report_ticks_ = ticks;
    }
}

void Simulation::run_commands() {
    std::vector<Command> commands;
    {
        const std::scoped_lock lock{mutex_};
        commands.swap(commands_);
    }
    for (const Command& command : commands) {
        try {
            command(instance_);
        } catch (const std::exception& error) {
            core::log_error("simulation: a command threw: {}", error.what());
        }
    }
}

void Simulation::post(Command command) {
    const std::scoped_lock lock{mutex_};
    commands_.push_back(std::move(command));
}

void Simulation::stop() {
    stopping_.store(true, std::memory_order_release);
    loop_.stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::uint64_t Simulation::ticks() const noexcept {
    return ticks_.load(std::memory_order_relaxed);
}

std::uint32_t Simulation::rate() const noexcept {
    return rate_;
}

} // namespace rtype
