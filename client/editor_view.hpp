#pragma once
#include <aegis2d/core/math.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/renderer.hpp>

#include "game_art.hpp"
#include "preferences.hpp"
#include <rtype/level_editor.hpp>

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

} // namespace rtype
