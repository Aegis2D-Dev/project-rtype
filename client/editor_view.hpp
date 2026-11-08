#pragma once
#include <aegis2d/core/math.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/renderer.hpp>

#include "game_art.hpp"
#include "menu_screens.hpp"
#include "preferences.hpp"
#include <rtype/level_editor.hpp>
#include <rtype/server_game.hpp>

#include <cstddef>
#include <string_view>

namespace rtype {

/// @brief Draws the level editor: the play area and the strip beyond its right edge, the
/// enemies of the wave edited (the one under the mouse highlighted), the mouse's cursor, and
/// the editor's text.
/// @param renderer Renderer in a frame.
/// @param font Font of the text.
/// @param art Sprites of the game.
/// @param editor Waves edited.
/// @param view Where the play area is drawn.
/// @param cursor Mouse, in logical pixels of the screen.
/// @param file Name of the waves file, shown at the top.
/// @param preferences Color vision and text size.
void draw_editor(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
                 const GameArt& art, const LevelEditor& editor, const EditorView& view,
                 aegis2d::Vec2 cursor, std::string_view file, const Preferences& preferences);

/// @brief Draws a message of the editor (saved, or why it could not save or play) above its
/// controls.
/// @param renderer Renderer in a frame.
/// @param font Font of the text.
/// @param message The sentence, and whether it is an error (in red).
/// @param preferences Color vision and text size.
void draw_editor_message(aegis2d::render::IRenderer& renderer,
                         const aegis2d::render::BitmapFont& font, const ScreenMessage& message,
                         const Preferences& preferences);

/// @brief Draws a playtest of the editor's waves: the world of the game played on the client,
/// the score and lives of the ship, and how to go back to the editor.
/// @param renderer Renderer in a frame.
/// @param font Font of the text.
/// @param art Sprites of the game.
/// @param game The game played.
/// @param ship The player's ship.
/// @param wave Index of the wave the playtest started from.
/// @param preferences Color vision and text size.
void draw_playtest(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
                   const GameArt& art, ServerGame& game, aegis2d::ecs::Entity ship,
                   std::size_t wave, const Preferences& preferences);

} // namespace rtype
