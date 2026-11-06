#include "editor_view.hpp"

#include <aegis2d/core/math.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/color.hpp>
#include <aegis2d/render/renderer.hpp>

#include "game_art.hpp"
#include "preferences.hpp"
#include <rtype/level_editor.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

namespace render = aegis2d::render;
using aegis2d::Vec2;

namespace {

constexpr std::uint32_t background_color = 0x0A0A1A;
constexpr std::uint32_t area_color = 0x16162E;
constexpr std::uint32_t strip_color = 0x24243F;
constexpr std::uint32_t text_color = 0xFFFFFF;
constexpr std::uint32_t dim_color = 0x9090A0;
constexpr std::uint32_t picked_color = 0xF0E442;
constexpr std::int32_t area_layer = 0;
constexpr std::int32_t enemy_layer = 10;
constexpr std::int32_t text_layer = 20;
// Size of the art on screen in game, as the game view draws it.
constexpr float art_scale = 3.0F;

render::Color color(std::uint32_t rgb, std::size_t vision) {
    return seen_as(render::rgb(rgb), vision);
}

} // namespace

void draw_editor(render::IRenderer& renderer, const render::BitmapFont& font, const GameArt& art,
                 const LevelEditor& editor, const EditorView& view, Vec2 cursor,
                 std::string_view file, const Preferences& preferences) {
    const std::size_t vision = preferences.vision;
    const float text = 2.0F * preferences.text_size.scale();
    renderer.draw({.position = {0.0F, 0.0F},
                   .size = {tuning::area_width, tuning::area_height},
                   .tint = color(background_color, vision),
                   .layer = area_layer - 1});
    // The play area, then the strip beyond its right edge where the enemies come from.
    const Vec2 corner = view.to_screen({0.0F, 0.0F});
    const Vec2 area = Vec2{tuning::area_width, tuning::area_height} * view.scale;
    renderer.draw(
        {.position = corner, .size = area, .tint = color(area_color, vision), .layer = area_layer});
    renderer.draw({.position = view.to_screen({tuning::area_width, 0.0F}),
                   .size = Vec2{EditorView::strip, tuning::area_height} * view.scale,
                   .tint = color(strip_color, vision),
                   .layer = area_layer});

    const Vec2 size = art.frame_size(Art::Bydos) * art_scale * view.scale;
    const std::optional<std::size_t> picked = editor.hovered();
    std::size_t index = 0;
    for (const DraftEnemy& enemy : editor.enemies()) {
        const Vec2 at = view.to_screen(enemy.position);
        const bool lit = picked == index;
        renderer.draw({.texture = art.texture(Art::Bydos, vision),
                       .position = at - (size / 2.0F),
                       .size = size,
                       .source = art.source(Art::Bydos, 0),
                       .tint = color(lit ? picked_color : text_color, vision),
                       .layer = enemy_layer});
        font.draw(renderer, std::format("{:.1f}S", enemy.at),
                  at + Vec2{-size.x / 2.0F, size.y / 2.0F},
                  {.scale = text / 2.0F,
                   .color = color(lit ? picked_color : dim_color, vision),
                   .layer = text_layer});
        ++index;
    }
    // A cross on the mouse, over the play area.
    const render::Color cross = color(picked_color, vision);
    renderer.draw({.position = cursor - Vec2{8.0F, 1.0F},
                   .size = {16.0F, 2.0F},
                   .tint = cross,
                   .layer = text_layer});
    renderer.draw({.position = cursor - Vec2{1.0F, 8.0F},
                   .size = {2.0F, 16.0F},
                   .tint = cross,
                   .layer = text_layer});

    // The timeline: a block per wave, from its start to its last enemy, the one edited lit.
    const std::vector<WaveSpan> spans = editor.timeline();
    const float seconds = EditorView::timeline_seconds(spans.back().end);
    const float per_second = view.timeline_size.x / seconds;
    renderer.draw({.position = view.timeline,
                   .size = view.timeline_size,
                   .tint = color(area_color, vision),
                   .layer = area_layer});
    // A mark every 5 seconds.
    const auto marks = static_cast<int>(seconds / 5.0F);
    for (int mark = 0; mark <= marks; ++mark) {
        renderer.draw(
            {.position = view.timeline + Vec2{static_cast<float>(mark) * 5.0F * per_second, 0.0F},
             .size = {1.0F, view.timeline_size.y},
             .tint = color(strip_color, vision),
             .layer = area_layer + 1});
    }
    for (std::size_t wave = 0; wave < spans.size(); ++wave) {
        const WaveSpan span = spans.at(wave);
        const bool edited = wave == editor.wave();
        const float width = std::max((span.end - span.start) * per_second, 4.0F);
        renderer.draw({.position = view.timeline + Vec2{span.start * per_second, 4.0F},
                       .size = {width, view.timeline_size.y - 8.0F},
                       .tint = color(edited ? picked_color : dim_color, vision),
                       .layer = enemy_layer});
    }
    const WaveSpan edited = spans.at(editor.wave());
    font.draw(renderer,
              std::format("DELAY {:.1f}S  FROM {:.1f}S TO {:.1f}S  OF {:.1f}S",
                          editor.draft().waves.at(editor.wave()).delay, edited.start, edited.end,
                          spans.back().end),
              view.timeline - Vec2{0.0F, font.line_height(text) + 2.0F},
              {.scale = text,
               .color = color(editor.dragging_wave() ? picked_color : dim_color, vision),
               .layer = text_layer});

    const std::size_t waves = editor.draft().waves.size();
    const std::string title =
        std::format("LEVEL EDITOR  {}  WAVE {}/{}  {} ENEMIES{}", file, editor.wave() + 1, waves,
                    editor.enemies().size(), editor.changed() ? "  (CHANGED)" : "");
    font.draw(renderer, title, {16.0F, 16.0F},
              {.scale = text, .color = color(text_color, vision), .layer = text_layer});
    const std::string help = "CLICK: PLACE, MOVE  RIGHT CLICK: REMOVE  PAGE UP/DOWN: WAVE  "
                             "LEFT/RIGHT: DELAY  UP/DOWN: ORDER  ESC: QUIT";
    font.draw(renderer, help, {16.0F, tuning::area_height - 16.0F - font.line_height(text)},
              {.scale = text, .color = color(dim_color, vision), .layer = text_layer});
}

} // namespace rtype
