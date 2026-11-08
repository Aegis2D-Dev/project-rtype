/// @file
/// @brief The best scores of the server, kept in a JSON file across restarts.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

/// @brief One score of the board.
struct ScoreEntry {
    /// @brief Name of the player, as valid_player_name() accepts it.
    std::string name;
    /// @brief Points at the end of its game.
    std::uint32_t score = 0;

    /// @brief Field-by-field equality.
    /// @return `true` if both fields are equal.
    bool operator==(const ScoreEntry&) const = default;
};

/// @brief The max_entries best scores, best first, kept in a JSON file.
/// @details The file is `{"scores": [{"name": "R9", "score": 1200}, ...]}`. It is read once
/// by the constructor: a missing file is an empty board; a file that cannot be read or is
/// not a board (bad JSON, a field of the wrong type, a bad name) is reported by
/// load_error() and logged, and the board starts empty, which never stops the server. Each
/// record() that changes the board writes it at once, through a temporary file that then
/// replaces the old one (`core::Config::save()`), so a crash never leaves half a file; a
/// write that fails is logged and the board goes on in memory. Not thread-safe.
class Scoreboard {
public:
    /// @brief Most scores kept.
    static constexpr std::size_t max_entries = 10;

    /// @brief Reads the board of a file.
    /// @param file Where the board is kept; created at the first record.
    explicit Scoreboard(std::filesystem::path file);

    /// @brief Adds a score, if it is among the best.
    /// @details An equal score goes after the ones already there.
    /// @param name Name of the player; an invalid name is not recorded.
    /// @param score Its points.
    /// @return `true` if the score is on the board, and the file was written.
    bool record(std::string_view name, std::uint32_t score);

    /// @brief The best scores.
    /// @return At most max_entries, best first.
    [[nodiscard]] const std::vector<ScoreEntry>& best() const noexcept;

    /// @brief Why the file could not be read at start.
    /// @return The reason, or `std::nullopt` if it was read or missing.
    [[nodiscard]] const std::optional<std::string>& load_error() const noexcept;

    /// @brief Where the board is kept.
    /// @return The file given to the constructor.
    [[nodiscard]] const std::filesystem::path& file() const noexcept;

private:
    void load();
    void save() const;

    std::filesystem::path file_;
    std::vector<ScoreEntry> best_;
    std::optional<std::string> load_error_;
};

} // namespace rtype
