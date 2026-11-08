/// @file
/// @brief TCP messages of the scoreboard (RFC section 3.9): encoding and decoding.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rtype {

/// @brief Most scores in a `ScoreList`.
inline constexpr std::size_t max_listed_scores = 10;

/// @brief One score of a `ScoreList`.
struct ScoreRecord {
    /// @brief Name of the player, as received: see valid_player_name().
    std::string name;
    /// @brief Its points at the end of its game.
    std::uint32_t score = 0;

    /// @brief Field-by-field equality.
    /// @return `true` if both fields are equal.
    bool operator==(const ScoreRecord&) const = default;
};

/// @brief Tells whether a message is a well-formed `ListScores`.
/// @param message Message received, length prefix excluded.
/// @return `true` for a `ListScores`, with no field.
[[nodiscard]] bool read_list_scores(std::span<const std::byte> message) noexcept;

/// @brief Reads a `ScoreList`.
/// @param message Message received, length prefix excluded.
/// @return The scores as received, best first, or `std::nullopt` if the message is not a
/// well-formed `ScoreList` (a `count` above max_listed_scores included).
[[nodiscard]] std::optional<std::vector<ScoreRecord>>
read_score_list(std::span<const std::byte> message);

/// @brief Encodes a `ListScores`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_list_scores();

/// @brief Encodes a `ScoreList`.
/// @param scores Best first; only the first max_listed_scores are written, and the names
/// must fit a `string`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_score_list(std::span<const ScoreRecord> scores);

} // namespace rtype
