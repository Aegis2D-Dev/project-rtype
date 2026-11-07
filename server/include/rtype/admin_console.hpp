/// @file
/// @brief The admin console of `r-type_server`: commands typed on its standard input.
#pragma once
#include <aegis2d/core/commands.hpp>

#include <rtype/server_network.hpp>

#include <functional>
#include <istream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

/// @brief The commands of the server's administrator, on the engine's command system.
/// @details
/// - `rooms`: the rooms in use, their players and whether they play;
/// - `players`: every session, with its room (or the lobby), its number and its address;
/// - `say <text>`: a message of the server to every player (RFC section 3.8);
/// - `close <room>`: ends the sessions of a room, each told `RoomClosed`;
/// - `stop`: stops the server, as Ctrl+C does (the `stop` given, request_stop() in
///   `r-type_server`);
/// - `help`, `help <command>`: what the commands do.
///
/// A wrong command, a missing or bad argument, or a command that cannot be done (a room not
/// in use, a text too long) says why and changes nothing. Use it on the network's thread.
class AdminConsole {
public:
    /// @brief Creates the commands.
    /// @param network The server's sockets and sessions; it must outlive the console.
    /// @param stop What `stop` does, such as request_stop().
    AdminConsole(ServerNetwork& network, std::function<void()> stop);

    /// @brief Runs a line typed by the administrator.
    /// @param line Such as `"close 1"`.
    /// @return How it went, and the text to print.
    aegis2d::core::CommandResult run(std::string_view line);

private:
    ServerNetwork* network_;
    std::function<void()> stop_;
    aegis2d::core::Commands commands_;
};

/// @brief The lines typed on a stream, read on a thread of their own, for the network
/// thread to take between two updates.
/// @details Reading blocks until a line comes, so the thread is detached and only touches
/// the lines it shares with this object: the process may end while it waits. It ends at the
/// end of the stream (Ctrl+D, Ctrl+Z, or a closed pipe).
class ConsoleInput {
public:
    /// @brief Starts reading.
    /// @param input Stream to read, such as `std::cin`; it must outlive the reading.
    explicit ConsoleInput(std::istream& input);

    /// @brief The lines read since the last call.
    /// @return Them, oldest first. Thread-safe.
    [[nodiscard]] std::vector<std::string> take();

    /// @brief Tells whether the stream ended.
    /// @return `true` once every line was read. Thread-safe.
    [[nodiscard]] bool ended() const;

private:
    struct Shared;
    std::shared_ptr<Shared> shared_;
};

} // namespace rtype
