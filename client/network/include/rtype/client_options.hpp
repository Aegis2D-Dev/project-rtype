/// @file
/// @brief Command line of `r-type_client`: server, player name, help and version.
#pragma once
#include <rtype/client_session.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace rtype {

/// @brief Usage line printed by `--help` and after a command line error.
inline constexpr std::string_view client_usage =
    "usage: r-type_client [--host <address>] [--port <tcp port>] [--name <player name>]\n"
    "                     [--join] [--settings <file>] [--frames <count>]\n"
    "                     [--screenshot <png file>] [--metrics] [--help] [--version]\n";

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
    /// (127.0.0.1, port 4242, `player`). They fill the connect screen.
    ClientSettings settings;
    /// @brief `--join`: join the server at once, without waiting on the connect screen.
    bool join = false;
    /// @brief `--settings`: the player's settings file (keys, auto-fire, text size,
    /// effects), instead of the one of the player's settings folder.
    std::optional<std::filesystem::path> settings_file;
    /// @brief `--frames`: quit after this many frames; 0 runs until the window closes.
    std::uint64_t frames = 0;
    /// @brief `--screenshot`: saves the frame before the last one as a PNG file (after
    /// `--frames`, or 300 frames by default).
    std::optional<std::filesystem::path> screenshot;
    /// @brief `--metrics`: shows the frame metrics from the start, as F12 does.
    bool metrics = false;
};

/// @brief Reads a TCP port, as `--port` and the connect screen take it.
/// @param text Decimal digits only: no sign, space or base prefix.
/// @return The port, from 1 to 65535, or nothing.
[[nodiscard]] std::optional<std::uint16_t> parse_port(std::string_view text) noexcept;

/// @brief Reads the command line of `r-type_client`.
/// @details Options may come in any order; a repeated option keeps its last value.
/// `--help` and `--version` stop the reading. The port is a number from 1 to 65535, the
/// frames a number from 1; the address and the name are checked when the client joins
/// (ClientSession::start()).
/// @param args Arguments after the program name.
/// @return The options, or a one-line message naming the faulty argument.
[[nodiscard]] std::variant<ClientOptions, std::string>
parse_client_options(std::span<const std::string_view> args);

} // namespace rtype
