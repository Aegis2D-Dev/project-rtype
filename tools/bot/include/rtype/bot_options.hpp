/// @file
/// @brief Command line of `r-type_bot`: server, name, play time, seed, help and version.
#pragma once
#include <rtype/bot.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace rtype {

/// @brief Usage line printed by `--help` and after a command line error.
inline constexpr std::string_view bot_usage =
    "usage: r-type_bot [--host <address>] [--port <tcp port>] [--name <player name>]\n"
    "                  [--seconds <play time>] [--seed <number>] [--help] [--version]\n";

/// @brief What the bot was asked to do.
enum class BotCommand : std::uint8_t {
    /// @brief Join the server and play.
    Run,
    /// @brief Print the usage and exit.
    Help,
    /// @brief Print the version and exit.
    Version,
};

/// @brief Settings of a bot run, read from the command line.
struct BotOptions {
    /// @brief What to do; the settings only matter for BotCommand::Run.
    BotCommand command = BotCommand::Run;
    /// @brief Server, name (`bot` by default) and seed.
    BotSettings settings;
    /// @brief `--seed` was given: otherwise each run draws its own.
    bool seeded = false;
    /// @brief `--seconds`: leave after playing this long; nothing plays until Ctrl+C.
    std::optional<std::uint32_t> seconds;
};

/// @brief Reads the command line of `r-type_bot`.
/// @details Options may come in any order; a repeated option keeps its last value.
/// `--help` and `--version` stop the reading. The port is a number from 1 to 65535, the
/// seconds a number from 1, the seed any 32-bit number; the address and the name are
/// checked when the bot joins (ClientSession::start()).
/// @param args Arguments after the program name.
/// @return The options, or a one-line message naming the faulty argument.
[[nodiscard]] std::variant<BotOptions, std::string>
parse_bot_options(std::span<const std::string_view> args);

} // namespace rtype
