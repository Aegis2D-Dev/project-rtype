/// @file
/// @brief The whole R-Type server: its sockets, its game on the simulation thread, and what
/// goes between them.
#pragma once
#include <aegis2d/net/context.hpp>

#include <rtype/event_tracker.hpp>
#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/server_network.hpp>
#include <rtype/simulation.hpp>
#include <rtype/tuning.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace rtype {

/// @brief Runs a game for the players of its sockets, as `r-type_server` does.
/// @details The sockets work on the caller's context, the game on its own Simulation
/// thread, and they only talk through posted tasks:
/// - a player whose UDP endpoint is bound joins the game: the others get its `PlayerJoined`,
///   and it gets a `PlayerJoined` of every player already there, then a `Spawn` of every
///   entity (RFC section 4.6);
/// - a player whose session ends (`Logout`, `Disconnect`, TCP closed, UDP timeout, protocol
///   error) leaves it: the others get its `PlayerLeft` with the reason, then, at the next
///   tick, the `Destroy` of its ship;
/// - the most recent `Input` of each player steers its ship;
/// - after each tick, the events of the tick go to every bound player, and every third tick
///   a snapshot;
/// - with each snapshot, the round trip time of each player sets how far back its missiles
///   meet the enemies (lag compensation, rewind_ticks()), when it changes.
///
/// The caller runs the context and calls `network().update()` a few times per second.
/// Destroying the server stops the simulation thread; the context must not run the tasks
/// left in it afterwards.
class GameServer {
public:
    /// @brief Opens both ports and starts the game.
    /// @param context Context of the sockets; it must outlive this object.
    /// @param instance Game to run; it must outlive this object and belongs to the
    /// simulation thread from now on.
    /// @param tcp_port TCP port of the logins, or 0 for any free port.
    /// @param udp_port UDP port of the game, or 0 for any free port.
    /// @param rate Ticks per second.
    /// @throws aegis2d::net::SocketError If a port is already in use or cannot be bound.
    GameServer(aegis2d::net::NetworkContext& context, GameInstance& instance,
               std::uint16_t tcp_port, std::uint16_t udp_port,
               std::uint32_t rate = tuning::tick_rate);

    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;
    GameServer(GameServer&&) = delete;
    GameServer& operator=(GameServer&&) = delete;
    /// @brief Stops the simulation thread, then closes the sockets.
    ~GameServer() = default;

    /// @brief The sockets and the sessions; use them on the context's thread.
    /// @return The network side.
    [[nodiscard]] ServerNetwork& network() noexcept;

    /// @brief The thread running the game.
    /// @return The simulation.
    [[nodiscard]] Simulation& simulation() noexcept;

private:
    void after_tick(GameInstance& instance);
    void on_joined(const Session& session);
    void on_left(const Session& session, LeaveReason reason);
    void update_rewinds();

    aegis2d::net::NetworkContext& context_;
    ServerNetwork network_;
    // Used by the simulation thread only.
    EventTracker events_;
    // Names of the players in the game, by number; used by the simulation thread only.
    std::array<std::optional<std::string>, tuning::max_players> names_{};
    // Tokens of the players in the game, by number, and the last rewind posted to the game;
    // used by the network thread only.
    std::array<std::optional<std::uint32_t>, tuning::max_players> tokens_{};
    std::array<std::uint32_t, tuning::max_players> rewinds_{};
    // Last: its thread starts once the rest is ready, and stops first.
    Simulation simulation_;
};

} // namespace rtype
