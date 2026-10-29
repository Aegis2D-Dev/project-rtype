/// @file
/// @brief Command line of `r-type_bot`: server, name, bots, inputs, play time, seed, help
/// and version.
#pragma once
#include <rtype/bot.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace rtype {

/// @brief Usage line printed by `--help` and after a command line error.
inline constexpr std::string_view bot_usage =
    "usage: r-type_bot [--host <address>] [--port <tcp port>] [--name <player name>]\n"
    "                  [--count <bots>] [--inputs <script file>] [--seconds <play time>]\n"
    "                  [--seed <number>] [--help] [--version]\n";

/// @brief Most bots `--count` runs in one process.
inline constexpr std::uint32_t max_bots = 64;

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
    /// @brief `--count`: number of bots, 1 to max_bots, named and seeded by bot_team().
    std::uint32_t count = 1;
    /// @brief `--inputs`: script replayed by every bot instead of random inputs, read with
    /// load_input_script().
    std::optional<std::filesystem::path> inputs;
};

/// @brief Reads the command line of `r-type_bot`.
/// @details Options may come in any order; a repeated option keeps its last value.
/// `--help` and `--version` stop the reading. The port is a number from 1 to 65535, the
/// seconds a number from 1, the count a number from 1 to max_bots, the seed any 32-bit
/// number; the address and the name are checked when the bot joins (ClientSession::start()),
/// the script when it is loaded.
/// @param args Arguments after the program name.
/// @return The options, or a one-line message naming the faulty argument.
[[nodiscard]] std::variant<BotOptions, std::string>
parse_bot_options(std::span<const std::string_view> args);

} // namespace rtype
