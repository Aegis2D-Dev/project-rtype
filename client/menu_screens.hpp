/// @file
/// @brief The screens around a game: connect, rooms, a room, joining, connection lost.
#pragma once
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/renderer.hpp>
#include <aegis2d/shmup/ship_controls.hpp>
#include <aegis2d/ui/text_field.hpp>
#include <aegis2d/ui/widget_tree.hpp>

#include "preferences.hpp"
#include <rtype/chat.hpp>
#include <rtype/connect_fields.hpp>
#include <rtype/connect_form.hpp>
#include <rtype/player_notices.hpp>
#include <rtype/room_list_form.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

/// @brief The ship's actions listed on the connect screen, each a row the player can
/// select to change its key.
inline constexpr std::array<aegis2d::ecs::Action, 5> control_rows{
    aegis2d::ecs::Action::Up, aegis2d::ecs::Action::Down, aegis2d::ecs::Action::Left,
    aegis2d::ecs::Action::Right, aegis2d::ecs::Action::Fire};

/// @brief Rows of the connect screen: the fields of ConnectForm, then control_rows.
inline constexpr std::size_t connect_rows = connect_field_count + control_rows.size();

/// @brief A line under the connect screen's fields: what happened last.
struct ScreenMessage {
    /// @brief The sentence, empty for none.
    std::string text;
    /// @brief Whether it is an error, drawn in red.
    bool error = false;
};

/// @brief The state of the connect screen besides its fields.
struct ConnectView {
    /// @brief Selected row, below connect_rows.
    std::size_t row = 0;
    /// @brief Whether the selected control waits for its new key.
    bool capturing = false;
};

/// @brief Draws the connect screen (#123): the goal of the game, the server, port and
/// name fields, the player's controls, which Enter changes, and the keys of the screen.
/// @details The fields are drawn by their text fields (Aegis2D-Dev/aegis2d#345), each in
/// its row's box: the text, scrolled to keep the cursor in view, and the cursor of the one
/// being edited.
/// @param renderer Renderer, between begin_frame() and end_frame().
/// @param font Font of the texts.
/// @param fields The fields; their boxes are set to the rows' places.
/// @param message Last message, under the fields.
/// @param view Selected row.
/// @param preferences The player's settings: keys, text size, color vision.
void draw_connect(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
                  ConnectFields& fields, const ScreenMessage& message, const ConnectView& view,
                  const Preferences& preferences);

/// @brief Most rooms the room list screen shows at once, around the selected one.
inline constexpr std::size_t visible_rooms = 9;

/// @brief Draws the room list screen (#304): the rooms of the server with their players and
/// state, the row of a new room (name and size), and the keys of the screen.
/// @param renderer Renderer, between begin_frame() and end_frame().
/// @param font Font of the texts.
/// @param form The rooms, the selected row and the new room.
/// @param message Last message, under the rooms.
/// @param seconds Seconds since the start, for the blinking cursor.
/// @param preferences The player's settings: text size, color vision.
void draw_rooms(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
                const RoomListForm& form, const ScreenMessage& message, double seconds,
                const Preferences& preferences);

/// @brief Draws the screen of the room joined (#305): its players, whether each is ready, and
/// the keys of the screen.
/// @param renderer Renderer, between begin_frame() and end_frame().
/// @param font Font of the texts.
/// @param name Name of the room.
/// @param room Its players, from the server's last `RoomPlayers`.
/// @param self Number of the player in the room.
/// @param message Last message, under the players.
/// @param preferences The player's settings: text size, color vision.
void draw_room(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
               std::string_view name, const RoomPlayers& room, std::uint8_t self,
               const ScreenMessage& message, const Preferences& preferences);

/// @brief Most characters of the chat line: 4 bytes each at most, so a message always fits
/// the server's limit (max_chat_size bytes).
inline constexpr std::size_t chat_length = max_chat_size / 4;

/// @brief Where the chat line goes while it is typed, at the bottom left.
inline constexpr aegis2d::render::Rect chat_box{
    .x = 20.0F, .y = 670.0F, .width = 640.0F, .height = 32.0F};

/// @brief Draws the last chat messages above the chat line, each in its sender's color, and
/// the line itself while it is typed.
/// @param renderer Renderer, between begin_frame() and end_frame().
/// @param font Font of the texts.
/// @param lines Messages to show, oldest first (ChatLog::shown()).
/// @param field The chat line, drawn when @p typing.
/// @param tree The tree of the chat line.
/// @param typing Whether the player is typing a message.
/// @param preferences The player's settings: text size, color vision.
void draw_chat(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
               const std::vector<PlayerNotice>& lines, const aegis2d::ui::TextField& field,
               const aegis2d::ui::WidgetTree& tree, bool typing, const Preferences& preferences);

/// @brief Draws a screen of one message: joining, or the connection lost (#128).
/// @param renderer Renderer, between begin_frame() and end_frame().
/// @param font Font of the texts.
/// @param title Title, in large letters.
/// @param text Explanation, wrapped to the screen.
/// @param footer What the player can do, at the bottom.
/// @param preferences The player's settings: text size, color vision.
void draw_status(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
                 std::string_view title, std::string_view text, std::string_view footer,
                 const Preferences& preferences);

} // namespace rtype
