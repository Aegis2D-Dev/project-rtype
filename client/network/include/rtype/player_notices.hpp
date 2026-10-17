/// @file
/// @brief The messages shown when a player joins or leaves the game.
#pragma once
#include <rtype/client_world.hpp>
#include <rtype/events.hpp>
#include <rtype/tuning.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rtype {

/// @brief A message about a player, shown for a few seconds.
struct PlayerNotice {
    /// @brief Player number, 0 to 3, for the color of its slot.
    std::uint8_t player = 0;
    /// @brief Sentence shown, such as `"Lolo left the game"`.
    std::string text;
    /// @brief When the message goes away.
    std::chrono::steady_clock::time_point until;
};

/// @brief Turns `PlayerJoined` and `PlayerLeft` events into messages for the players who
/// stay.
/// @details A `PlayerLeft` only carries the player's number, and ClientWorld forgets the
/// player when it applies the event: remember() keeps the names of the players of the last
/// frame, so the message can name who left. The players already there when this client
/// joined are not announced: the server tells their names with the first events, all of the
/// same tick, so a `PlayerJoined` of that tick or older is only remembered.
class PlayerNotices {
public:
    /// @brief Clock of the messages.
    using Clock = std::chrono::steady_clock;
    /// @brief How long a message stays.
    static constexpr std::chrono::seconds shown_for{5};
    /// @brief Most messages shown at once: a new one pushes out the oldest.
    static constexpr std::size_t max_shown = 4;

    /// @brief Adds the message of an event, if it is a `PlayerJoined` or a `PlayerLeft`.
    /// @param event Event applied by ClientWorld (ClientWorld::take_events()).
    /// @param now Time it is shown from.
    void handle(const GameEvent& event, Clock::time_point now);

    /// @brief Keeps the names of the players, for the next `PlayerLeft`.
    /// @details Call it once the events of a frame are handled. A player without a name yet
    /// keeps the name it had.
    /// @param players Players of the world (ClientWorld::players()).
    void remember(const std::array<std::optional<RemotePlayer>, tuning::max_players>& players);

    /// @brief Drops the messages whose time is over.
    /// @param now Current time.
    void prune(Clock::time_point now);

    /// @brief Messages to show, oldest first.
    /// @return The messages.
    [[nodiscard]] const std::vector<PlayerNotice>& shown() const noexcept;

    /// @brief Forgets the messages and names, for a new session.
    void clear() noexcept;

private:
    void add(std::uint8_t player, std::string text, Clock::time_point now);
    [[nodiscard]] std::string name_of(std::uint8_t player) const;

    std::array<std::string, tuning::max_players> names_;
    std::vector<PlayerNotice> shown_;
    std::optional<std::uint32_t> first_tick_;
};

/// @brief Sentence telling why a player left.
/// @param name Name of the player.
/// @param reason Why it left, as `PlayerLeft` gives it.
/// @return `"<name> left the game"`, `"<name> lost the connection"` or
/// `"<name> was removed from the game"`.
[[nodiscard]] std::string left_message(const std::string& name, LeaveReason reason);

} // namespace rtype
