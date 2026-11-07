#include "menu_screens.hpp"

#include <aegis2d/core/math.hpp>
#include <aegis2d/platform/actions.hpp>
#include <aegis2d/platform/controls.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/color.hpp>
#include <aegis2d/render/renderer.hpp>
#include <aegis2d/shmup/ship_controls.hpp>
#include <aegis2d/ui/text_field.hpp>

#include "game_art.hpp"
#include "preferences.hpp"
#include <rtype/connect_fields.hpp>
#include <rtype/connect_form.hpp>
#include <rtype/room_list_form.hpp>
#include <rtype/rooms.hpp>
#include <rtype/scores.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
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
                  ConnectFields& fields, const ScreenMessage& message, const ConnectView& view,
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
    const render::Rect box_at{.x = left + label_width - 8.0F,
                              .y = 0.0F,
                              .width = box_width + 16.0F,
                              .height = row_height - 2.0F};
    const auto label_row = [&](std::size_t index, std::string_view name) {
        render::TextStyle name_style = label;
        if (index == view.row) {
            name_style.color = color(title_color, preferences);
        }
        font.draw(renderer, name, {left, y}, name_style);
    };
    const auto row = [&](std::size_t index, std::string_view name, const std::string& text) {
        renderer.draw({.position = {box_at.x, y - 4.0F},
                       .size = {box_at.width, box_at.height},
                       .tint = color(index == view.row ? 0x3A3A6A : box_color, preferences),
                       .layer = 0});
        label_row(index, name);
        font.draw(renderer, text, {left + label_width, y}, value);
        y += row_height;
    };
    // The fields: text fields in the rows' boxes, the one being edited outlined, with its
    // cursor; text longer than its box scrolls with the cursor.
    const aegis2d::ui::TextFieldStyle field_style{.background = color(box_color, preferences),
                                                  .focused = color(0x3A3A6A, preferences),
                                                  .disabled = color(box_color, preferences),
                                                  .text = value.color,
                                                  .placeholder = color(dim_color, preferences),
                                                  .cursor = color(title_color, preferences),
                                                  .outline = color(title_color, preferences),
                                                  .outline_width = 2.0F,
                                                  .text_scale = value.scale,
                                                  .layer = 0};
    for (std::size_t field = 0; field < connect_field_count; ++field) {
        render::Rect box = box_at;
        box.y = y - 4.0F;
        fields.draw(renderer, font, static_cast<ConnectField>(field), box, field_style);
        label_row(field, field_labels.at(field));
        y += row_height;
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

void draw_rooms(render::IRenderer& renderer, const render::BitmapFont& font,
                const RoomListForm& form, const ScreenMessage& message,
                std::span<const ScoreRecord> scores, double seconds,
                const Preferences& preferences) {
    const render::TextStyle title = style(5.0F, title_color, preferences);
    const render::TextStyle body = style(2.5F, text_color, preferences);
    const render::TextStyle small = style(2.0F, dim_color, preferences);
    float y = gap;
    font.draw(renderer, "ROOMS", {centered_x(font, "ROOMS", title.scale), y}, title);
    y += font.line_height(title.scale) + gap;

    // The rooms around the selected one, then the row of a new room; the selected row is boxed.
    const std::vector<RoomRecord>& rooms = form.rooms();
    const float left = 4.0F * gap;
    const float row_height = font.line_height(body.scale) + 10.0F;
    const auto row = [&](bool selected, const std::string& text) {
        renderer.draw({.position = {left - 8.0F, y - 4.0F},
                       .size = {width - (2.0F * left) + 16.0F, row_height - 2.0F},
                       .tint = color(selected ? 0x3A3A6A : box_color, preferences),
                       .layer = 0});
        render::TextStyle text_style = body;
        if (selected) {
            text_style.color = color(title_color, preferences);
        }
        font.draw(renderer, text, {left, y}, text_style);
        y += row_height;
    };
    if (rooms.empty()) {
        font.draw(renderer, "NO ROOM YET: CREATE ONE BELOW", {left, y}, small);
        y += row_height;
    }
    const std::size_t first =
        form.row() < visible_rooms ? 0 : std::min(form.row(), rooms.size()) + 1 - visible_rooms;
    for (std::size_t index = first; index < rooms.size() && index < first + visible_rooms;
         ++index) {
        const RoomRecord& room = rooms.at(index);
        row(index == form.row(),
            std::format("{:<16}  {}/{} PLAYERS  {}", upper(room.name), room.players,
                        room.max_players,
                        room.state == static_cast<std::uint8_t>(RoomState::Waiting) ? "WAITING"
                                                                                    : "PLAYING"));
    }
    const bool cursor = form.creating() && std::fmod(seconds, 1.0) < 0.5;
    row(form.creating(),
        std::format("NEW ROOM: {:<17} {} PLAYERS", form.name() + (cursor ? "_" : ""), form.size()));
    y += gap / 2.0F;

    if (!message.text.empty()) {
        const render::TextStyle said =
            style(2.0F, message.error ? error_color : title_color, preferences);
        const std::string text = font.wrap(message.text, width - (4.0F * gap), said.scale);
        font.draw(renderer, text, {centered_x(font, text, said.scale), y}, said);
        y += font.line_height(said.scale) + gap;
    }

    // The best scores of the server (RFC section 3.9), in two columns of 5, best first.
    if (!scores.empty()) {
        const render::TextStyle heading = style(2.5F, heading_color, preferences);
        font.draw(renderer, "BEST SCORES", {left, y}, heading);
        y += font.line_height(heading.scale) + 6.0F;
        const render::TextStyle line = style(2.0F, text_color, preferences);
        const float line_height = font.line_height(line.scale) + 4.0F;
        constexpr std::size_t per_column = 5;
        std::size_t index = 0;
        for (const ScoreRecord& score : scores) {
            const float x = left + (index < per_column ? 0.0F : (width / 2.0F) - left);
            const float at = y + (static_cast<float>(index % per_column) * line_height);
            ++index;
            font.draw(renderer,
                      std::format("{:>2}. {:<16} {:>7}", index, upper(score.name), score.score),
                      {x, at}, line);
        }
    }
    constexpr std::string_view keys =
        "ENTER JOIN OR CREATE   UP / DOWN CHOOSE   LEFT / RIGHT PLAYERS   ESC LEAVE";
    font.draw(renderer, keys,
              {centered_x(font, keys, small.scale), height - font.line_height(small.scale) - gap},
              small);
}

void draw_room(render::IRenderer& renderer, const render::BitmapFont& font, std::string_view name,
               const RoomPlayers& room, std::uint8_t self, const ScreenMessage& message,
               const Preferences& preferences) {
    const render::TextStyle title = style(5.0F, title_color, preferences);
    const render::TextStyle body = style(2.5F, text_color, preferences);
    const render::TextStyle small = style(2.0F, dim_color, preferences);
    float y = gap;
    const std::string heading = std::format("ROOM {}", upper(name));
    font.draw(renderer, heading, {centered_x(font, heading, title.scale), y}, title);
    y += font.line_height(title.scale) + gap;

    // One row per player, the player's own highlighted, each ready or not.
    const float left = 4.0F * gap;
    const float row_height = font.line_height(body.scale) + 10.0F;
    for (const RoomPlayer& player : room.players) {
        renderer.draw({.position = {left - 8.0F, y - 4.0F},
                       .size = {width - (2.0F * left) + 16.0F, row_height - 2.0F},
                       .tint = color(player.player == self ? 0x3A3A6A : box_color, preferences),
                       .layer = 0});
        render::TextStyle text_style = body;
        text_style.color = color(player.ready ? title_color : text_color, preferences);
        font.draw(renderer,
                  std::format("PLAYER {}  {:<16}  {}{}", player.player + 1, upper(player.name),
                              player.ready ? "READY" : "NOT READY",
                              player.player == self ? "  (YOU)" : ""),
                  {left, y}, text_style);
        y += row_height;
    }
    y += gap / 2.0F;
    const auto ready = std::ranges::count_if(room.players, &RoomPlayer::ready);
    const std::string waiting = std::format("{} OF {} READY: THE GAME STARTS ONCE EVERY PLAYER IS",
                                            ready, room.players.size());
    font.draw(renderer, waiting, {centered_x(font, waiting, small.scale), y}, small);
    y += font.line_height(small.scale) + gap;

    if (!message.text.empty()) {
        const render::TextStyle said =
            style(2.0F, message.error ? error_color : title_color, preferences);
        const std::string text = font.wrap(message.text, width - (4.0F * gap), said.scale);
        font.draw(renderer, text, {centered_x(font, text, said.scale), y}, said);
    }
    constexpr std::string_view keys = "ENTER READY / NOT READY   ESC LEAVE THE ROOM";
    font.draw(renderer, keys,
              {centered_x(font, keys, small.scale), height - font.line_height(small.scale) - gap},
              small);
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
