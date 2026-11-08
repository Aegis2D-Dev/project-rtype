/// @file
/// @brief The last chat messages a client shows, each for a few seconds.
#pragma once
#include <rtype/chat.hpp>
#include <rtype/player_notices.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rtype {

/// @brief The chat messages to show, newest last, each for shown_for.
/// @details A line is a PlayerNotice: the sender's number gives its color (lobby_player for
/// a sender in the lobby, and for what the server refused), its text is `"name: text"`. A
/// message whose name or text breaks the rules of the protocol is ignored (RFC section 3.8).
class ChatLog {
public:
    /// @brief Clock of the lines.
    using Clock = std::chrono::steady_clock;
    /// @brief How long a line stays.
    static constexpr std::chrono::seconds shown_for{8};
    /// @brief Most lines shown at once: a new one pushes out the oldest.
    static constexpr std::size_t max_shown = 6;

    /// @brief Adds a message relayed by the server.
    /// @param message Its sender and text.
    /// @param now Time it is shown from.
    /// @return `false` if it was ignored.
    bool add(const ChatMessage& message, Clock::time_point now);

    /// @brief Adds the reason a message of this client was not relayed.
    /// @param reason `reason` of the `ChatRejected`, as received.
    /// @param now Time it is shown from.
    void refused(std::uint8_t reason, Clock::time_point now);

    /// @brief Drops the lines whose time is over.
    /// @param now Current time.
    void prune(Clock::time_point now);

    /// @brief Lines to show, oldest first.
    /// @return The lines.
    [[nodiscard]] const std::vector<PlayerNotice>& shown() const noexcept;

    /// @brief Forgets every line, for a new session.
    void clear() noexcept;

private:
    void push(PlayerNotice line);

    std::vector<PlayerNotice> shown_;
};

/// @brief Sentence telling why a message was not sent.
/// @param reason `reason` of a `ChatRejected`, unknown values included.
/// @return `"Message not sent: too long"`, `"... too fast"`...
[[nodiscard]] std::string chat_rejection_text(std::uint8_t reason);

} // namespace rtype
