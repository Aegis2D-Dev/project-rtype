/// @file
/// @brief What a bot measured of its link to the server, for `r-type_bot --stats`.
#pragma once
#include <rtype/bot.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace rtype {

/// @brief The link of one bot over its game, as the network benchmark reads it.
struct BotStats {
    /// @brief Player name of the bot.
    std::string name;
    /// @brief Time in game.
    double seconds = 0.0;
    /// @brief `Input` datagrams sent.
    std::uint64_t inputs_sent = 0;
    /// @brief `Snapshot` datagrams received.
    std::uint64_t snapshots = 0;
    /// @brief Bytes received per second in game, headers of the protocol included.
    double bytes_in_per_second = 0.0;
    /// @brief Bytes sent per second in game, headers of the protocol included.
    double bytes_out_per_second = 0.0;
    /// @brief Smoothed round trip time at the end, in milliseconds.
    double rtt_ms = 0.0;
    /// @brief Its jitter, in milliseconds.
    double rtt_jitter_ms = 0.0;
    /// @brief Share of the server's datagrams lost over the last 2 seconds, in percent.
    double loss_percent = 0.0;
    /// @brief Jitter of the snapshots' arrivals over the last 2 seconds, in milliseconds.
    double snapshot_jitter_ms = 0.0;
};

/// @brief First line of a `--stats` file: the names of the columns of bot_stats_line().
inline constexpr std::string_view bot_stats_header =
    "bot,seconds,inputs_sent,snapshots,bytes_in_per_s,bytes_out_per_s,rtt_ms,rtt_jitter_ms,"
    "loss_percent,snapshot_jitter_ms";

/// @brief Reads what a bot measured, before it leaves.
/// @param bot The bot.
/// @param name Its player name.
/// @param played Time it spent in game.
/// @param now Current time, the end of the windows of the loss and the snapshot jitter.
/// @return The figures.
[[nodiscard]] BotStats measure_bot(const Bot& bot, std::string name,
                                   std::chrono::steady_clock::duration played,
                                   std::chrono::steady_clock::time_point now);

/// @brief One line of a `--stats` file, in the order of bot_stats_header, without its end.
/// @param stats Figures of a bot.
/// @return The comma-separated values, the decimals with a dot.
[[nodiscard]] std::string bot_stats_line(const BotStats& stats);

} // namespace rtype
