#include "menu_screens.hpp"

#include <aegis2d/core/math.hpp>
#include <aegis2d/platform/actions.hpp>
#include <aegis2d/platform/controls.hpp>
#include <aegis2d/platform/ship_controls.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/color.hpp>
#include <aegis2d/render/renderer.hpp>

#include "game_art.hpp"
#include "preferences.hpp"
#include <rtype/connect_form.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace rtype {

namespace platform = aegis2d::platform;
namespace render = aegis2d::render;
using aegis2d::Vec2;

namespace {

constexpr float width = tuning::area_width;
constexpr float height = tuning::area_height;
constexpr float gap = 20.0F;
constexpr std::uint32_t title_color = 0xF0E442;
constexpr std::uint32_t heading_color = 0x66CCEE;
constexpr std::uint32_t text_color = 0xFFFFFF;
constexpr std::uint32_t dim_color = 0x9090A0;
constexpr std::uint32_t error_color = 0xFF8080;
constexpr std::uint32_t box_color = 0x20203A;
constexpr std::array<std::string_view, connect_field_count> field_labels{"SERVER", "PORT", "NAME"};
constexpr std::array<std::string_view, control_rows.size()> control_labels{
    "MOVE UP", "MOVE DOWN", "MOVE LEFT", "MOVE RIGHT", "FIRE"};

render::Color color(std::uint32_t rgb, const Preferences& preferences) {
    return seen_as(render::rgb(rgb), preferences.vision);
}

// A style at the player's text size, never wider than the screen allows.
render::TextStyle style(float scale, std::uint32_t rgb, const Preferences& preferences) {
    return preferences.text_size.apply(
        {.scale = scale, .color = color(rgb, preferences), .layer = 1});
}

std::string upper(std::string_view text) {
    std::string result{text};
    std::ranges::transform(result, result.begin(), [](char character) {
        return character >= 'a' && character <= 'z' ? static_cast<char>(character - 'a' + 'A')
                                                    : character;
    });
    return result;
}

float centered_x(const render::BitmapFont& font, std::string_view text, float scale) {
    return (width - font.measure(text, scale).x) / 2.0F;
}

} // namespace

void draw_connect(render::IRenderer& renderer, const render::BitmapFont& font,
                  const ConnectForm& form, const ScreenMessage& message, const ConnectView& view,
                  const Preferences& preferences) {
    constexpr std::string_view goal_text =
        "Fly through the Bydo empire with up to four players. Shoot the enemies, dodge their "
        "missiles and keep your lives.";
    const std::string keys = std::format(
        "ENTER JOIN (ON A CONTROL: CHANGE ITS KEY)   TAB / UP / DOWN CHOOSE   ESC QUIT\n"
        "F1 AUTO-FIRE {}   F2 FLASH/SHAKE {}   F3 VISION {}   F4/F5 TEXT {}%",
        preferences.controls.auto_fire().enabled() ? "ON" : "OFF",
        preferences.effects.flashes || preferences.effects.shake ? "ON" : "OFF",
        upper(vision_names.at(preferences.vision % vision_count)), preferences.text_size.percent());
    const std::string widest_value(ConnectForm::max_host / 2, 'M');
    const auto keys_scale = [&](float factor) {
        return std::min(2.0F * factor,
                        (width - (2.0F * gap)) / std::max(font.measure(keys).x, 1.0F));
    };
    // The player's text size, made smaller in steps until the screen holds everything.
    const auto fits = [&](float factor) {
        const float rows =
            (static_cast<float>(connect_rows) * (font.line_height(2.5F * factor) + 10.0F)) +
            font.line_height(2.0F * factor) + 8.0F + gap;
        const float total =
            gap + font.line_height(6.0F * factor) + gap +
            font.measure(font.wrap(goal_text, width - (8.0F * gap), 2.0F * factor), 2.0F * factor)
                .y +
            gap + rows + (2.0F * font.line_height(2.0F * factor)) +
            font.measure(keys, keys_scale(factor)).y + (2.0F * gap);
        const float row_width = font.measure("MOVE RIGHT  ", 2.5F * factor).x +
                                font.measure(widest_value, 2.5F * factor).x;
        return total <= height && row_width <= width - (2.0F * gap);
    };
    float factor = preferences.text_size.scale();
    while (factor > render::TextSize::smallest && !fits(factor)) {
        factor -= render::TextSize::step;
    }
    const auto sized = [&](float scale, std::uint32_t rgb) {
        return render::TextStyle{
            .scale = scale * factor, .color = color(rgb, preferences), .layer = 1};
    };

    float y = gap;
    const render::TextStyle title = sized(6.0F, title_color);
    font.draw(renderer, "R-TYPE", {centered_x(font, "R-TYPE", title.scale), y}, title);
    y += font.line_height(title.scale) + gap;

    const render::TextStyle body = sized(2.0F, text_color);
    const std::string goal = font.wrap(goal_text, width - (8.0F * gap), body.scale);
    font.draw(renderer, goal, {centered_x(font, goal, body.scale), y}, body);
    y += font.measure(goal, body.scale).y + gap;

    // The rows: the three fields, then the ship's controls; the selected one is boxed.
    const render::TextStyle label = sized(2.5F, heading_color);
    const render::TextStyle value = sized(2.5F, text_color);
    const float row_height = font.line_height(label.scale) + 10.0F;
    const float label_width = font.measure("MOVE RIGHT  ", label.scale).x;
    const float box_width = font.measure(widest_value, value.scale).x;
    const float left = (width - label_width - box_width) / 2.0F;
    const auto row = [&](std::size_t index, std::string_view name, const std::string& text) {
        const bool selected = index == view.row;
        renderer.draw({.position = {left + label_width - 8.0F, y - 4.0F},
                       .size = {box_width + 16.0F, row_height - 2.0F},
                       .tint = color(selected ? 0x3A3A6A : box_color, preferences),
                       .layer = 0});
        render::TextStyle name_style = label;
        if (selected) {
            name_style.color = color(title_color, preferences);
        }
        font.draw(renderer, name, {left, y}, name_style);
        font.draw(renderer, text, {left + label_width, y}, value);
        y += row_height;
    };
    const bool cursor = std::fmod(view.seconds, 1.0) < 0.5;
    for (std::size_t field = 0; field < connect_field_count; ++field) {
        const auto which = static_cast<ConnectField>(field);
        std::string text = form.text(which);
        if (view.row == field && cursor) {
            text += '_';
        }
        row(field, field_labels.at(field), text);
    }
    y += gap / 2.0F;
    const render::TextStyle heading = sized(2.0F, dim_color);
    font.draw(renderer, "YOUR CONTROLS", {left, y}, heading);
    y += font.line_height(heading.scale) + 8.0F;
    const platform::ActionMap& actions = preferences.controls.actions();
    for (std::size_t index = 0; index < control_rows.size(); ++index) {
        const platform::ActionId action = preferences.controls.id(control_rows.at(index));
        const std::size_t at = connect_field_count + index;
        const std::string bound = view.capturing && view.row == at
                                      ? std::string{"PRESS A KEY (ESCAPE CANCELS)"}
                                      : platform::keys_label(actions, action);
        row(at, control_labels.at(index), bound);
    }
    y += gap / 2.0F;

    if (!message.text.empty()) {
        const render::TextStyle said = sized(2.0F, message.error ? error_color : title_color);
        const std::string text = font.wrap(message.text, width - (4.0F * gap), said.scale);
        font.draw(renderer, text, {centered_x(font, text, said.scale), y}, said);
    }

    // The keys of the screen, at the bottom.
    const float scale = keys_scale(factor);
    const Vec2 keys_size = font.measure(keys, scale);
    font.draw(renderer, keys, {(width - keys_size.x) / 2.0F, height - keys_size.y - gap},
              {.scale = scale, .color = color(dim_color, preferences), .layer = 1});
}

void draw_status(render::IRenderer& renderer, const render::BitmapFont& font,
                 std::string_view title, std::string_view text, std::string_view footer,
                 const Preferences& preferences) {
    const render::TextStyle big = style(5.0F, title_color, preferences);
    const render::TextStyle body = style(2.5F, text_color, preferences);
    const render::TextStyle small = style(2.0F, dim_color, preferences);
    const std::string wrapped = font.wrap(text, width - (8.0F * gap), body.scale);
    const float total =
        font.line_height(big.scale) + font.measure(wrapped, body.scale).y + (2.0F * gap);
    float y = (height - total) / 2.0F;
    font.draw(renderer, title, {centered_x(font, title, big.scale), y}, big);
    y += font.line_height(big.scale) + (2.0F * gap);
    font.draw(renderer, wrapped, {centered_x(font, wrapped, body.scale), y}, body);
    const float footer_y = height - font.line_height(small.scale) - (2.0F * gap);
    font.draw(renderer, footer, {centered_x(font, footer, small.scale), footer_y}, small);
}

} // namespace rtype
