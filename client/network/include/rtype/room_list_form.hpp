/// @file
/// @brief The room list screen apart from drawing it: the rooms, the row selected, and the
/// room to create.
#pragma once
#include <rtype/rooms.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace rtype {

/// @brief What the player does on the room list screen.
/// @details The rows are the rooms of the last update(), by number, then the row of a new
/// room, whose name the player types and whose size (1 to 4 players) it chooses. submit()
/// gives the room to join on a room row, or the room to create on the last row. Nothing here
/// draws or reads the keyboard, so the screen is the same with any window.
class RoomListForm {
public:
    /// @brief Longest room name, in bytes, as `CreateRoom` allows.
    static constexpr std::size_t max_name = 16;

    /// @brief Shows the rooms the server listed.
    /// @details The selected room stays selected while it is listed; the row of a new room
    /// stays the last one.
    /// @param rooms Rooms of the last `RoomList`, by number.
    void update(std::span<const RoomRecord> rooms);

    /// @brief The rooms shown.
    /// @return Them, by number.
    [[nodiscard]] const std::vector<RoomRecord>& rooms() const noexcept;

    /// @brief The selected row: a room, or rooms().size() for the row of a new room.
    /// @return The row.
    [[nodiscard]] std::size_t row() const noexcept;

    /// @brief Tells whether the row of a new room is selected.
    /// @return `true` on the last row.
    [[nodiscard]] bool creating() const noexcept;

    /// @brief Selects the row above, the last one after the first.
    void up() noexcept;

    /// @brief Selects the row below, the first one after the last.
    void down() noexcept;

    /// @brief Adds a character to the name of the new room.
    /// @param character Character typed: printable ASCII, space included.
    /// @return `false` if the row of a new room is not selected, the name is full or the
    /// character is not printable; the name is unchanged.
    bool type(char character);

    /// @brief Removes the last character of the name of the new room, if any.
    void erase() noexcept;

    /// @brief One more player in the new room, up to 4.
    void bigger() noexcept;

    /// @brief One less player in the new room, down to 1.
    void smaller() noexcept;

    /// @brief Name of the new room.
    /// @return The text typed.
    [[nodiscard]] const std::string& name() const noexcept;

    /// @brief Size of the new room.
    /// @return 1 to 4, 4 at first.
    [[nodiscard]] std::uint8_t size() const noexcept;

    /// @brief What Enter does on the selected row.
    /// @return The number of the room to join, the room to create, or a sentence telling the
    /// player why neither: a full room, or a new room without a name.
    [[nodiscard]] std::variant<std::uint8_t, CreateRoomRequest, std::string> submit() const;

private:
    std::vector<RoomRecord> rooms_;
    std::size_t row_ = 0;
    std::string name_;
    std::uint8_t size_ = 4;
};

} // namespace rtype
