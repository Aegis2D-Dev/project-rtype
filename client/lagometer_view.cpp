#include "lagometer_view.hpp"

#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/color.hpp>
#include <aegis2d/render/renderer.hpp>

#include "game_art.hpp"
#include "preferences.hpp"
#include <rtype/lagometer.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace rtype {

namespace render = aegis2d::render;

namespace {

constexpr float bar_width = 2.0F;
constexpr float graph_width = bar_width * static_cast<float>(Lagometer::length);
constexpr float graph_height = 28.0F;
constexpr float label_scale = 2.0F;
constexpr float padding = 8.0F;
constexpr float gap = 6.0F;
constexpr float top = 40.0F;
constexpr float margin = 16.0F;
// Over the world and the HUD.
constexpr std::int32_t panel_layer = 110;

constexpr std::uint32_t good_color = 0x60D060;
constexpr std::uint32_t warning_color = 0xE0C040;
constexpr std::uint32_t bad_color = 0xFF5050;
constexpr std::uint32_t bandwidth_color = 0x60A0FF;
constexpr std::uint32_t text_color = 0xFFFFFF;

render::Color color_of(LagGraph graph, double value, std::size_t vision) {
    if (graph == LagGraph::BytesIn || graph == LagGraph::BytesOut) {
        return seen_as(render::rgb(bandwidth_color), vision);
    }
    switch (lag_level(graph, value)) {
    case LagLevel::Good:
        return seen_as(render::rgb(good_color), vision);
    case LagLevel::Warning:
        return seen_as(render::rgb(warning_color), vision);
    case LagLevel::Bad:
        return seen_as(render::rgb(bad_color), vision);
    }
    return seen_as(render::rgb(text_color), vision);
}

} // namespace

void draw_lagometer(render::IRenderer& renderer, const render::BitmapFont& font,
                    const Lagometer& lagometer, const Preferences& preferences) {
    const std::size_t vision = preferences.vision;
    const float row = font.line_height(label_scale) + 2.0F + graph_height + gap;
    const float left = tuning::area_width - graph_width - (2.0F * padding) - margin;
    renderer.draw({.position = {left, top},
                   .size = {graph_width + (2.0F * padding),
                            (row * static_cast<float>(lag_graph_count)) - gap + (2.0F * padding)},
                   .tint = render::Color{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.7F},
                   .layer = panel_layer});
    const LagSample latest = lagometer.latest();
    for (std::size_t index = 0; index < lag_graph_count; ++index) {
        const auto graph = static_cast<LagGraph>(index);
        const float x = left + padding;
        const float y = top + padding + (row * static_cast<float>(index));
        font.draw(renderer, lag_label(graph, latest.value(graph)), {x, y},
                  {.scale = label_scale,
                   .color = color_of(graph, latest.value(graph), vision),
                   .layer = panel_layer + 1});
        const float base = y + font.line_height(label_scale) + 2.0F + graph_height;
        renderer.draw({.position = {x, base - 1.0F},
                       .size = {graph_width, 1.0F},
                       .tint = render::Color{.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = 0.25F},
                       .layer = panel_layer + 1});
        // Newest on the right: the graph fills from the right at the start of a session.
        const double scale = lagometer.scale(graph);
        float bar_x =
            x + graph_width - (bar_width * static_cast<float>(lagometer.samples().size()));
        for (const LagSample& sample : lagometer.samples()) {
            const auto height =
                static_cast<float>(std::clamp(sample.value(graph) / scale, 0.0, 1.0) *
                                   static_cast<double>(graph_height));
            if (height > 0.0F) {
                renderer.draw({.position = {bar_x, base - height},
                               .size = {bar_width, height},
                               .tint = color_of(graph, sample.value(graph), vision),
                               .layer = panel_layer + 1});
            }
            bar_x += bar_width;
        }
    }
}

} // namespace rtype
