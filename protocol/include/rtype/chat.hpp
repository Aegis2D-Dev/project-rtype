/// @file
/// @brief TCP messages of the chat (RFC section 3.8): encoding, decoding and text rules.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

/// @brief Longest chat text, in bytes of UTF-8.
inline constexpr std::size_t max_chat_size = 200;

/// @brief `player` of a `ChatMessage` whose sender is in the lobby, with no player number.
inline constexpr std::uint8_t lobby_player = 255;

/// @brief Why a chat message is not relayed, the `reason` of `ChatRejected`.
enum class ChatRejection : std::uint8_t {
    /// @brief The text is longer than max_chat_size bytes.
    TooLong = 1,
    /// @brief The text is empty, not UTF-8 or has control characters.
    BadText = 2,
    /// @brief The sender is over the rate: the message is a flood.
    TooFast = 3,
};

/// @brief Content of a `ChatMessage`.
struct ChatMessage {
    /// @brief Number of the sender in its room, or lobby_player.
    std::uint8_t player = 0;
    /// @brief Name of the sender, as received: see valid_player_name().
    std::string name;
    /// @brief Text, as received: see check_chat_text().
    std::string text;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const ChatMessage&) const = default;
};

/// @brief Short name of a rejection, for logs.
/// @param reason Reason to name.
/// @return `"too long"`, `"bad text"`, `"too fast"` or `"unknown"`.
[[nodiscard]] std::string_view to_string(ChatRejection reason) noexcept;

/// @brief Checks a text against the rules of `ChatSend`, the length first.
/// @param text Bytes received.
/// @return `std::nullopt` if the text may be relayed, or ChatRejection::TooLong or
/// ChatRejection::BadText.
[[nodiscard]] std::optional<ChatRejection> check_chat_text(std::string_view text) noexcept;

/// @brief Reads a `ChatSend`.
/// @param message Message received, length prefix excluded.
/// @return The text as received, or `std::nullopt` if the message is not a well-formed
/// `ChatSend`.
[[nodiscard]] std::optional<std::string> read_chat_send(std::span<const std::byte> message);

/// @brief Reads a `ChatMessage`.
/// @param message Message received, length prefix excluded.
/// @return Its fields as received, or `std::nullopt` if the message is not a well-formed
/// `ChatMessage`.
[[nodiscard]] std::optional<ChatMessage> read_chat_message(std::span<const std::byte> message);

/// @brief Reads a `ChatRejected`.
/// @param message Message received, length prefix excluded.
/// @return The reason as sent (unknown values included), or `std::nullopt` if the message
/// is not a well-formed `ChatRejected`.
[[nodiscard]] std::optional<std::uint8_t>
read_chat_rejected(std::span<const std::byte> message) noexcept;

/// @brief Encodes a `ChatSend`.
/// @param text Text to send; it must fit a `string`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_chat_send(std::string_view text);

/// @brief Encodes a `ChatMessage`.
/// @param message Sender and text; the strings must fit a `string`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_chat_message(const ChatMessage& message);

/// @brief Encodes a `ChatRejected`.
/// @param reason Why the message was not relayed.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_chat_rejected(ChatRejection reason);

} // namespace rtype
