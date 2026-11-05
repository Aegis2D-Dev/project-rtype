/// @file
/// @brief TCP messages of the rooms (RFC section 3.6): listing them and creating one.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

    /// @brief Field-by-field equality.
    /// @return `true` if both fields are equal.
    bool operator==(const CreateRoomRequest&) const = default;
};

/// @brief Short name of a rejection, for logs.
/// @param reason Reason to name.
/// @return `"no room left"`, `"bad name"`, `"bad size"` or `"unknown"`.
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

/// @brief Encodes a `ListRooms`.
/// @return The message, without length prefix.
[[nodiscard]] std::vector<std::byte> write_list_rooms();

/// @brief Encodes a `RoomList`.
/// @param rooms The rooms, at most max_listed_rooms, their names fitting a `string`.
/// @return The message, without length prefix.
/// @throws std::invalid_argument If there are more than max_listed_rooms rooms.
[[nodiscard]] std::vector<std::byte> write_room_list(std::span<const RoomRecord> rooms);

/// @brief Encodes a `CreateRoom`.
/// @param request Name and number of players; the name must fit a `string`.
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

} // namespace rtype
