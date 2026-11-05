/// @file
/// @brief TCP messages of the rooms (RFC sections 3.6 and 3.7): listing them, creating one,
/// joining and leaving one, and starting its game.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rtype {

/// @brief Most rooms a server may have, and a `RoomList` may hold.
inline constexpr std::size_t max_listed_rooms = 64;

/// @brief What a room is doing, the `state` of a `RoomList` record.
enum class RoomState : std::uint8_t {
    /// @brief None of its players is in game yet.
    Waiting = 0,
    /// @brief Its game is running.
    Playing = 1,
};

/// @brief Why a room is not created, the `reason` of `RoomRejected`.
enum class RoomRejection : std::uint8_t {
    /// @brief Every room of the server is in use.
    NoRoomLeft = 1,
    /// @brief The name is empty, too long, not UTF-8 or has control characters.
    BadName = 2,
    /// @brief The number of players is not 1 to 4.
    BadSize = 3,
    /// @brief The lives are not 1 to max_room_lives.
    BadLives = 4,
    /// @brief The difficulty is not a Difficulty.
    BadDifficulty = 5,
    /// @brief The server has no waves file of this name.
    UnknownWaves = 6,
    /// @brief The friendly fire is not 0 or 1.
    BadFriendlyFire = 7,
};

/// @brief Most lives a room may give its players.
inline constexpr std::uint8_t max_room_lives = 9;

/// @brief How hard the enemies of a room are: how often they come and how much they take.
enum class Difficulty : std::uint8_t {
    /// @brief Fewer enemies, easier to destroy.
    Easy = 0,
    /// @brief The game as tuned.
    Normal = 1,
    /// @brief More enemies, harder to destroy.
    Hard = 2,
};

/// @brief How a room plays, chosen at its creation (`CreateRoom`, RFC section 3.6).
/// @details The values are as received: the server checks them (SessionTable::create_room()).
struct RoomRules {
    /// @brief Lives of each player, 1 to max_room_lives.
    std::uint8_t lives = 3;
    /// @brief A Difficulty value.
    std::uint8_t difficulty = static_cast<std::uint8_t>(Difficulty::Normal);
    /// @brief Name of the waves file to play, among those the server offers, without its
    /// `.json`; empty for the server's own.
    std::string waves;
    /// @brief 1 if the players' missiles hit the other players, 0 if not.
    std::uint8_t friendly_fire = 0;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const RoomRules&) const = default;
};

/// @brief Why a room is not joined, the `reason` of `JoinRejected`.
enum class JoinRejection : std::uint8_t {
    /// @brief No room in use has this number.
    NoSuchRoom = 1,
    /// @brief The room has as many players as it takes.
    RoomFull = 2,
    /// @brief The player is in game: it plays in its room until its session ends.
    InGame = 3,
};

/// @brief A player of a room, as a `RoomPlayers` describes it.
struct RoomPlayer {
    /// @brief Number in the room, 0 to 3.
    std::uint8_t player = 0;
    /// @brief Name given at login.
    std::string name;
    /// @brief Whether it is ready to start.
    bool ready = false;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const RoomPlayer&) const = default;
};

/// @brief Content of a `RoomPlayers`.
struct RoomPlayers {
    /// @brief Number of the room.
    std::uint8_t room = 0;
    /// @brief Its players, at most 4, by number.
    std::vector<RoomPlayer> players;

    /// @brief Field-by-field equality.
    /// @return `true` if both fields are equal.
    bool operator==(const RoomPlayers&) const = default;
};

/// @brief A room, as a `RoomList` describes it.
struct RoomRecord {
    /// @brief Number of the room, 0 to max_listed_rooms - 1.
    std::uint8_t room = 0;
    /// @brief Name of the room.
    std::string name;
    /// @brief Players in the room.
    std::uint8_t players = 0;
    /// @brief Most players the room takes, 1 to 4.
    std::uint8_t max_players = 0;
    /// @brief State as sent: a RoomState value, or an unknown one.
    std::uint8_t state = 0;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const RoomRecord&) const = default;
};

/// @brief Content of a `CreateRoom`.
struct CreateRoomRequest {
    /// @brief Name of the room, not checked yet: see valid_player_name(), whose rules apply.
    std::string name;
    /// @brief Most players the room takes, not checked yet: 1 to 4 are valid.
    std::uint8_t max_players = 0;
    /// @brief How the room plays, not checked yet.
    RoomRules rules{};

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const CreateRoomRequest&) const = default;
};

/// @brief Short name of a rejection, for logs.
/// @param reason Reason to name.
/// @return `"no room left"`, `"bad name"`, `"bad size"`, `"bad lives"`... or `"unknown"`.
[[nodiscard]] std::string_view to_string(RoomRejection reason) noexcept;

/// @brief Tells whether a message is a well-formed `ListRooms`: its kind alone.
/// @param message Message received, length prefix excluded.
/// @return `true` if it is.
[[nodiscard]] bool read_list_rooms(std::span<const std::byte> message) noexcept;

/// @brief Reads a `RoomList`.
/// @param message Message received, length prefix excluded.
/// @return Its rooms, in order, or `std::nullopt` if the message is not a well-formed
/// `RoomList`: another kind, more than max_listed_rooms rooms, a field past the end, or bytes
/// after the last room.
[[nodiscard]] std::optional<std::vector<RoomRecord>>
read_room_list(std::span<const std::byte> message);

/// @brief Reads a `CreateRoom`.
/// @param message Message received, length prefix excluded.
/// @return Its fields, or `std::nullopt` if the message is not a well-formed `CreateRoom`.
[[nodiscard]] std::optional<CreateRoomRequest> read_create_room(std::span<const std::byte> message);

/// @brief Reads a `RoomCreated`.
/// @param message Message received, length prefix excluded.
/// @return The number of the room, or `std::nullopt` if the message is not a well-formed
/// `RoomCreated`.
[[nodiscard]] std::optional<std::uint8_t>
read_room_created(std::span<const std::byte> message) noexcept;

/// @brief Reads a `RoomRejected`.
/// @param message Message received, length prefix excluded.
/// @return The reason as sent (unknown values included), or `std::nullopt` if the message
/// is not a well-formed `RoomRejected`.
[[nodiscard]] std::optional<std::uint8_t>
read_room_rejected(std::span<const std::byte> message) noexcept;

/// @brief Short name of a rejection, for logs.
/// @param reason Reason to name.
/// @return `"no such room"`, `"room full"`, `"in game"` or `"unknown"`.
[[nodiscard]] std::string_view to_string(JoinRejection reason) noexcept;

/// @brief Reads a `JoinRoom`.
/// @param message Message received, length prefix excluded.
/// @return The number of the room, or `std::nullopt` if the message is not a well-formed
/// `JoinRoom`.
[[nodiscard]] std::optional<std::uint8_t>
read_join_room(std::span<const std::byte> message) noexcept;

/// @brief Reads a `RoomJoined`.
/// @param message Message received, length prefix excluded.
/// @return The room and the player's number there, or `std::nullopt` if the message is not a
/// well-formed `RoomJoined`.
[[nodiscard]] std::optional<std::pair<std::uint8_t, std::uint8_t>>
read_room_joined(std::span<const std::byte> message) noexcept;

/// @brief Reads a `JoinRejected`.
/// @param message Message received, length prefix excluded.
/// @return The reason as sent (unknown values included), or `std::nullopt` if the message
/// is not a well-formed `JoinRejected`.
[[nodiscard]] std::optional<std::uint8_t>
read_join_rejected(std::span<const std::byte> message) noexcept;

/// @brief Tells whether a message is a well-formed `LeaveRoom`: its kind alone.
/// @param message Message received, length prefix excluded.
/// @return `true` if it is.
[[nodiscard]] bool read_leave_room(std::span<const std::byte> message) noexcept;

/// @brief Reads a `SetReady`.
/// @param message Message received, length prefix excluded.
/// @return Whether the player is ready, or `std::nullopt` if the message is not a
/// well-formed `SetReady`, its `ready` being 0 or 1.
[[nodiscard]] std::optional<bool> read_set_ready(std::span<const std::byte> message) noexcept;

/// @brief Reads a `RoomPlayers`.
/// @param message Message received, length prefix excluded.
/// @return Its room and players, or `std::nullopt` if the message is not a well-formed
/// `RoomPlayers`: another kind, more than 4 players, a `ready` other than 0 or 1, a field
/// past the end, or bytes after the last player.
[[nodiscard]] std::optional<RoomPlayers> read_room_players(std::span<const std::byte> message);

/// @brief Tells whether a message is a well-formed `GameStart`: its kind alone.
/// @param message Message received, length prefix excluded.
/// @return `true` if it is.
[[nodiscard]] bool read_game_start(std::span<const std::byte> message) noexcept;

/// @brief Encodes a `ListRooms`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_list_rooms();

/// @brief Encodes a `RoomList`.
/// @param rooms The rooms, at most max_listed_rooms, their names fitting a `string`.
/// @return The message, without length prefix.
/// @throws std::invalid_argument If there are more than max_listed_rooms rooms.
[[nodiscard]] std::vector<std::byte> write_room_list(std::span<const RoomRecord> rooms);

/// @brief Encodes a `CreateRoom`.
/// @param request Name, number of players and rules; the strings must fit a `string`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_create_room(const CreateRoomRequest& request);

/// @brief Encodes a `RoomCreated`.
/// @param room Number of the room created.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_room_created(std::uint8_t room);

/// @brief Encodes a `RoomRejected`.
/// @param reason Why the room is not created.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_room_rejected(RoomRejection reason);

/// @brief Encodes a `JoinRoom`.
/// @param room Number of the room to join.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_join_room(std::uint8_t room);

/// @brief Encodes a `RoomJoined`.
/// @param room Number of the room joined.
/// @param player Number of the player there.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_room_joined(std::uint8_t room, std::uint8_t player);

/// @brief Encodes a `JoinRejected`.
/// @param reason Why the room is not joined.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_join_rejected(JoinRejection reason);

/// @brief Encodes a `LeaveRoom`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_leave_room();

/// @brief Encodes a `SetReady`.
/// @param ready Whether the player is ready to start.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_set_ready(bool ready);

/// @brief Encodes a `RoomPlayers`.
/// @param players The room and its players, at most 4, their names fitting a `string`.
/// @return The message, without length prefix.
/// @throws std::invalid_argument If there are more than 4 players.
[[nodiscard]] std::vector<std::byte> write_room_players(const RoomPlayers& players);

/// @brief Encodes a `GameStart`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_game_start();

} // namespace rtype
