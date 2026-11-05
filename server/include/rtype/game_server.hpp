/// @file
/// @brief The whole R-Type server: its sockets, the games of its rooms, each on its own
/// thread, and what goes between them.
#pragma once
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/context.hpp>

#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/room.hpp>
#include <rtype/server_network.hpp>
#include <rtype/simulation.hpp>
#include <rtype/tuning.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace rtype {

/// @brief Runs the games of its rooms for the players of its sockets, as `r-type_server` does.
/// @details The sockets work on the caller's context, each game in its Room, on the room's
/// Simulation thread, and they only talk through posted tasks. Each session plays in the
/// room of its Session::room, and everything below stays within that room:
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
/// The logins fill the first room with a free player number, then the next one, up to
/// max_rooms() rooms (SessionTable::set_rooms()). A room opens when its first player joins:
/// its game starts on a new thread. It closes when its last player leaves: its thread is
/// stopped and joined, and its game is gone, so the next player of that room starts a new
/// one. The caller runs the context and calls `network().update()` a few times per second.
/// Destroying the server stops the simulation threads; the context must not run the tasks
/// left in it afterwards.
class GameServer {
public:
    /// @brief Opens both ports; the rooms open as their players join.
    /// @param context Context of the sockets; it must outlive this object.
    /// @param seed Seed of the random spawns of the rooms' games, or `std::nullopt` for none.
    /// @param waves Waves the rooms play in every game, at their start.
    /// @param tcp_port TCP port of the logins, or 0 for any free port.
    /// @param udp_port UDP port of the game, or 0 for any free port.
    /// @param rate Ticks per second of each room.
    /// @param max_rooms Rooms played at once, at least 1.
    /// @throws aegis2d::net::SocketError If a port is already in use or cannot be bound.
    GameServer(aegis2d::net::NetworkContext& context, std::optional<std::uint64_t> seed,
               aegis2d::ecs::WaveSchedule waves, std::uint16_t tcp_port, std::uint16_t udp_port,
               std::uint32_t rate = tuning::tick_rate, std::uint32_t max_rooms = 1);

    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;
    GameServer(GameServer&&) = delete;
    GameServer& operator=(GameServer&&) = delete;
    /// @brief Stops the simulation threads, then closes the sockets.
    ~GameServer() = default;

    /// @brief The sockets and the sessions; use them on the context's thread.
    /// @return The network side.
    [[nodiscard]] ServerNetwork& network() noexcept;

    /// @brief An open room: its game, thread and event tracker. Use it on the context's
    /// thread: the room closes there when its last player leaves.
    /// @param id Number of the room, below max_rooms().
    /// @return The room.
    /// @throws std::out_of_range If the room is not open.
    [[nodiscard]] Room& room(std::uint32_t id = 0);

    /// @brief Number of rooms open, with a game running.
    /// @return 0 to max_rooms().
    [[nodiscard]] std::uint32_t open_rooms() const noexcept;

    /// @brief Number of rooms played at once.
    /// @return The count given to the constructor, at least 1.
    [[nodiscard]] std::uint32_t max_rooms() const noexcept;

    /// @brief Ticks per second of each room.
    /// @return The rate given to the constructor.
    [[nodiscard]] std::uint32_t rate() const noexcept;

    /// @brief The thread running the game of the first room, `room().simulation()`.
    /// @return The simulation.
    /// @throws std::out_of_range If the first room is not open.
    [[nodiscard]] Simulation& simulation();

private:
    // A player in a game: its room, its number there and the last rewind posted to the game.
    struct Seat {
        std::uint32_t room = 0;
        std::uint8_t player = 0;
        std::uint32_t rewind = 0;
    };

    void after_tick(Room& room, GameInstance& instance);
    void on_joined(const Session& session);
    void on_left(const Session& session, LeaveReason reason);
    void update_rewinds(std::uint32_t id);
    Room& open_room(std::uint32_t id);
    void close_room(std::uint32_t id);

    aegis2d::net::NetworkContext& context_;
    ServerNetwork network_;
    // What each room's game is built with.
    std::optional<std::uint64_t> seed_;
    aegis2d::ecs::WaveSchedule waves_;
    std::uint32_t rate_;
    // The players in a game, by token; used by the network thread only.
    std::unordered_map<std::uint32_t, Seat> seats_;
    // One slot per room, empty while it is closed; used by the network thread only. Last:
    // their threads start once the rest is ready, and stop first.
    std::vector<std::unique_ptr<Room>> rooms_;
};

} // namespace rtype
