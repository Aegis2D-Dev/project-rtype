/// @file
/// @brief Command line of `r-type_server`: ports, waves file, help and version.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace rtype {

/// @brief TCP port the server listens on when `--tcp-port` is not given.
inline constexpr std::uint16_t default_tcp_port = 4242;
/// @brief UDP port the server listens on when `--udp-port` is not given.
inline constexpr std::uint16_t default_udp_port = 4243;
/// @brief Datagrams accepted per second from one address when `--max-packet-rate` is not
/// given; see RateLimits.
inline constexpr std::uint32_t default_packet_rate = 300;
/// @brief Rooms played at once when `--max-rooms` is not given; see GameServer.
inline constexpr std::uint32_t default_max_rooms = 4;

/// @brief Usage line printed by `--help` and after a command line error.
inline constexpr std::string_view server_usage =
    "usage: r-type_server [--tcp-port <port>] [--udp-port <port>] [--waves <file.json>]\n"
    "                     [--max-packet-rate <datagrams per second>] [--compress <bytes>]\n"
    "                     [--max-rooms <count>] [--help] [--version]\n";

/// @brief What the server was asked to do.
enum class ServerCommand : std::uint8_t {
    /// @brief Start the game and listen on the ports.
    Run,
    /// @brief Print the usage and exit.
    Help,
    /// @brief Print the version and exit.
    Version,
};

/// @brief Settings of a server run, read from the command line.
struct ServerOptions {
    /// @brief What to do; the other fields only matter for ServerCommand::Run.
    ServerCommand command = ServerCommand::Run;
    /// @brief TCP port of the logins; 0 lets the system pick a free one.
    std::uint16_t tcp_port = default_tcp_port;
    /// @brief UDP port of the game; 0 lets the system pick a free one.
    std::uint16_t udp_port = default_udp_port;
    /// @brief Waves file to play, or `std::nullopt` for default_waves_file().
    std::optional<std::filesystem::path> waves;
    /// @brief Datagrams accepted per second from one address, 1 to 100000.
    std::uint32_t packet_rate = default_packet_rate;
    /// @brief Size from which snapshots and events are sent compressed with LZ4 when that
    /// makes them smaller, 1 to 1182 bytes, or `std::nullopt` (the default) to never compress.
    std::optional<std::size_t> compress_from;
    /// @brief Rooms played at once, 1 to 64, tuning::max_players in each.
    std::uint32_t max_rooms = default_max_rooms;
};

/// @brief Reads the command line of `r-type_server`.
/// @details Options may come in any order; a repeated option keeps its last value.
/// `--help` and `--version` stop the reading: what follows them is not checked. A port is
/// a decimal number from 0 to 65535, 0 asking the system for a free port.
/// @param args Arguments after the program name.
/// @return The options, or a one-line message naming the faulty argument.
[[nodiscard]] std::variant<ServerOptions, std::string>
parse_server_options(std::span<const std::string_view> args);

} // namespace rtype
