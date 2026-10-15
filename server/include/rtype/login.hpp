/// @file
/// @brief TCP messages of the session (RFC section 3.2): encoding, decoding and name rules.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

/// @brief Protocol version the server speaks, in `LoginRequest` and in the UDP header.
inline constexpr std::uint8_t protocol_version = 1;

/// @brief Longest player name, in bytes of UTF-8.
inline constexpr std::size_t max_name_size = 16;

/// @brief First byte of every TCP message: what it is.
/// @details The values are part of the protocol: never renumber them.
enum class TcpKind : std::uint8_t {
    /// @brief Client to server: version and name.
    LoginRequest = 1,
    /// @brief Server to client: session token, player number and UDP port.
    LoginAccepted = 2,
    /// @brief Server to client: why the login failed; the server closes afterwards.
    LoginRejected = 3,
    /// @brief Both ways: the sender ends the session and closes.
    Logout = 4,
};

/// @brief Why a login is refused, the `reason` of `LoginRejected`.
enum class LoginRejection : std::uint8_t {
    /// @brief The game already has 4 players.
    ServerFull = 1,
    /// @brief The server does not speak the requested version.
    BadVersion = 2,
    /// @brief The name is empty, too long, not UTF-8 or has control characters.
    BadName = 3,
    /// @brief Another player of the game has this name.
    NameTaken = 4,
    /// @brief The game is running and does not take new players.
    GameStarted = 5,
};

/// @brief Why a session ends, the `reason` of `Logout`.
enum class LogoutReason : std::uint8_t {
    /// @brief The player quit (sent by the client).
    Left = 0,
    /// @brief The server is stopping.
    Shutdown = 1,
    /// @brief No valid datagram for too long.
    TimedOut = 2,
    /// @brief The client sent something invalid over UDP.
    ProtocolError = 3,
};

/// @brief Content of a `LoginRequest`.
struct LoginRequest {
    /// @brief Protocol version of the client.
    std::uint8_t version = protocol_version;
    /// @brief Player name, not checked yet: see valid_player_name().
    std::string name;

    /// @brief Field-by-field equality.
    /// @return `true` if both fields are equal.
    bool operator==(const LoginRequest&) const = default;
};

/// @brief Content of a `LoginAccepted`.
struct LoginAccepted {
    /// @brief Session token, never 0.
    std::uint32_t session = 0;
    /// @brief Player number, 0 to 3.
    std::uint8_t player = 0;
    /// @brief UDP port of the server.
    std::uint16_t udp_port = 0;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const LoginAccepted&) const = default;
};

/// @brief Short name of a rejection, for logs.
/// @param reason Reason to name.
/// @return `"server full"`, `"bad version"`... or `"unknown"`.
[[nodiscard]] std::string_view to_string(LoginRejection reason) noexcept;

/// @brief Tells whether a name follows the rules of `LoginRequest`.
/// @details 1 to max_name_size bytes of valid UTF-8 (no overlong form, surrogate or code
/// point above U+10FFFF), without control characters (below U+0020, and U+007F).
/// @param name Bytes received.
/// @return `true` if the name can be given to a player.
[[nodiscard]] bool valid_player_name(std::string_view name) noexcept;

/// @brief Kind of a TCP message.
/// @param message Message received, length prefix excluded.
/// @return Its kind, or `std::nullopt` if it is empty or its kind is reserved.
[[nodiscard]] std::optional<TcpKind> kind_of(std::span<const std::byte> message) noexcept;

/// @brief Reads a `LoginRequest`.
/// @param message Message received, length prefix excluded.
/// @return The request, or `std::nullopt` if the message is not a well-formed
/// `LoginRequest`: another kind, a field past the end, or bytes after the name.
[[nodiscard]] std::optional<LoginRequest> read_login_request(std::span<const std::byte> message);

/// @brief Reads a `Logout`.
/// @param message Message received, length prefix excluded.
/// @return The reason as sent (unknown values included), or `std::nullopt` if the message
/// is not a well-formed `Logout`.
[[nodiscard]] std::optional<std::uint8_t> read_logout(std::span<const std::byte> message) noexcept;

/// @brief Encodes a `LoginRequest`.
/// @param request Version and name; the name must fit a `string`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_login_request(const LoginRequest& request);

/// @brief Encodes a `LoginAccepted`.
/// @param accepted Token, player and port.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_login_accepted(const LoginAccepted& accepted);

/// @brief Encodes a `LoginRejected`.
/// @param reason Why the login failed.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_login_rejected(LoginRejection reason);

/// @brief Encodes a `Logout`.
/// @param reason Why the session ends.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_logout(LogoutReason reason);

} // namespace rtype
