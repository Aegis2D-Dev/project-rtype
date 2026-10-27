/// @file
/// @brief The screens around a game: connect, joining, connection lost.
#pragma once
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/renderer.hpp>
#include <aegis2d/shmup/ship_controls.hpp>

#include "preferences.hpp"
#include <rtype/connect_form.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

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
    /// @brief Seconds since the start, for the blinking cursor.
    double seconds = 0.0;
};

/// @brief Draws the connect screen (#123): the goal of the game, the server, port and
/// name fields, the player's controls, which Enter changes, and the keys of the screen.
/// @param renderer Renderer, between begin_frame() and end_frame().
/// @param font Font of the texts.
/// @param form The fields.
/// @param message Last message, under the fields.
/// @param view Selected row and cursor.
/// @param preferences The player's settings: keys, text size, color vision.
void draw_connect(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
                  const ConnectForm& form, const ScreenMessage& message, const ConnectView& view,
                  const Preferences& preferences);

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
