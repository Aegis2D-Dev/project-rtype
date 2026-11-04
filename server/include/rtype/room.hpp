/// @file
/// @brief A game of the server, with the thread that runs it.
#pragma once
#include <aegis2d/ecs/waves.hpp>

#include <rtype/event_tracker.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/simulation.hpp>
#include <rtype/tuning.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace rtype {

/// @brief One game of the server: its GameInstance, the Simulation thread that runs it, the
/// EventTracker of what its clients learnt, and the names of its players.
/// @details The game, the tracker and the names belong to the simulation thread: other
/// threads post() to simulation() to use them, or wait until it stopped. The thread starts
/// once the rest of the room is built, and stops first when the room is destroyed. The
/// server keeps a single room for now; more rooms will run side by side, each on its own
/// thread.
class Room {
public:
    /// @brief Called on the simulation thread after each tick of the room.
    using TickHandler = std::function<void(Room& room, GameInstance& instance)>;

    /// @brief Creates the game and starts its thread.
    /// @param seed Seed of the random spawns, or `std::nullopt` for none (see ServerGame).
    /// @param waves Waves to play in every game of the room, at their start.
    /// @param rate Ticks per second.
    /// @param after_tick Called after each tick, on the simulation thread.
    Room(std::optional<std::uint64_t> seed, aegis2d::ecs::WaveSchedule waves, std::uint32_t rate,
         TickHandler after_tick);

    Room(const Room&) = delete;
    Room& operator=(const Room&) = delete;
    Room(Room&&) = delete;
    Room& operator=(Room&&) = delete;
    /// @brief Stops the thread, then destroys the game.
    ~Room() = default;

    /// @brief The game of the room.
    /// @return It; use it from the simulation thread, or once simulation() stopped.
    [[nodiscard]] GameInstance& instance() noexcept;

    /// @brief The thread running the game.
    /// @return The simulation.
    [[nodiscard]] Simulation& simulation() noexcept;

    /// @brief What the clients of the room learnt of its entities.
    /// @return The tracker; use it from the simulation thread.
    [[nodiscard]] EventTracker& events() noexcept;

    /// @brief Names of the players in the room's game, by number.
    /// @return An empty slot for a number with no player; use them from the simulation
    /// thread.
    [[nodiscard]] std::array<std::optional<std::string>, tuning::max_players>& names() noexcept;

private:
    GameInstance instance_;
    EventTracker events_;
    std::array<std::optional<std::string>, tuning::max_players> names_{};
    // Last: its thread starts once the rest is ready, and stops first.
    Simulation simulation_;
};

} // namespace rtype
