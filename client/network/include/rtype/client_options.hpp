/// @file
/// @brief Command line of `r-type_client`: server, player name, help and version.
#pragma once
#include <rtype/client_session.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace rtype {

/// @brief Usage line printed by `--help` and after a command line error.
inline constexpr std::string_view client_usage =
    "usage: r-type_client [--host <address>] [--port <tcp port>] [--name <player name>]\n"
    "                     [--help] [--version]\n";

/// @brief What the client was asked to do.
enum class ClientCommand : std::uint8_t {
    /// @brief Join the server.
    Run,
    /// @brief Print the usage and exit.
    Help,
    /// @brief Print the version and exit.
    Version,
};

/// @brief Settings of a client run, read from the command line.
struct ClientOptions {
    /// @brief What to do; the settings only matter for ClientCommand::Run.
    ClientCommand command = ClientCommand::Run;
    /// @brief Server and player name; ClientSettings gives the defaults
    /// (127.0.0.1, port 4242, `player`).
    ClientSettings settings;
};

/// @brief Reads the command line of `r-type_client`.
/// @details Options may come in any order; a repeated option keeps its last value.
/// `--help` and `--version` stop the reading. The port is a number from 1 to 65535; the
/// address and the name are checked when the client starts (ClientSession::start()).
/// @param args Arguments after the program name.
/// @return The options, or a one-line message naming the faulty argument.
[[nodiscard]] std::variant<ClientOptions, std::string>
parse_client_options(std::span<const std::string_view> args);

} // namespace rtype
