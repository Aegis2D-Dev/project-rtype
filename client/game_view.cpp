#include "game_view.hpp"

#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/event_bus.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/animation.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/ecs/system.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/color.hpp>
#include <aegis2d/render/player_style.hpp>
#include <aegis2d/render/renderer.hpp>
#include <aegis2d/render/screen_effects.hpp>
#include <aegis2d/render/starfield.hpp>

#include "game_art.hpp"
#include "preferences.hpp"
#include <rtype/client_world.hpp>
#include <rtype/events.hpp>
#include <rtype/player_notices.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>

namespace rtype {

namespace core = aegis2d::core;
namespace ecs = aegis2d::ecs;
namespace render = aegis2d::render;
using aegis2d::Vec2;

namespace {

constexpr float width = tuning::area_width;
constexpr float height = tuning::area_height;
// Each pixel of the art is drawn as a square of this size.
constexpr float art_scale = 3.0F;
constexpr float marker_size = 20.0F;
constexpr float hud_height = 64.0F;
constexpr std::size_t hud_name_length = 10;
constexpr float margin = 16.0F;
constexpr std::int32_t world_layer = 10;
constexpr std::int32_t hud_layer = 100;
constexpr std::uint32_t background_color = 0x0A0A1A;
constexpr std::uint32_t enemy_missile_color = 0xFF6060;
constexpr std::uint32_t text_color = 0xFFFFFF;
constexpr std::uint32_t dim_color = 0x9090A0;
// Frames of the sheets' animations: the flame of a ship flickers, a Bydos moves its legs.
constexpr double ship_frame_time = 0.06;
constexpr double bydos_frame_time = 0.25;
constexpr float explosion_frame_time = 0.06F;
// How long the keys of the game show: at the start of a game, then after a change.
constexpr double help_at_start = 10.0;
constexpr double help_after_change = 4.0;

render::Color color(std::uint32_t rgb, std::size_t vision) {
    return seen_as(render::rgb(rgb), vision);
}

render::Color faded(render::Color base, float alpha) {
    base.a = alpha;
    return base;
}

render::Color player_color(std::size_t player, std::size_t vision) {
    return seen_as(render::player_style(player).color, vision);
}

// Frame of a looping animation at a time; each entity starts at its own frame, from its id,
// so that a wave of Bydos does not move its legs in step.
std::uint32_t frame_at(double seconds, double frame_time, std::uint32_t id) {
    return static_cast<std::uint32_t>(std::floor(seconds / frame_time)) + id;
}

std::string upper(std::string text) {
    std::ranges::transform(text, text.begin(), [](char character) {
        return character >= 'a' && character <= 'z' ? static_cast<char>(character - 'a' + 'A')
                                                    : character;
    });
    return text;
}

} // namespace

GameView::GameView(const GameArt& art, const render::BitmapFont& font, core::EventBus& events)
    : art_{art}, font_{font}, events_{events}, stars_{Vec2{width, height}} {}

void GameView::start(std::uint8_t player, std::string name) {
    blasts_.clear();
    notices_.clear();
    kinds_.clear();
    effects_ = render::ScreenEffects{effects_.settings()};
    player_ = player;
    name_ = std::move(name);
    help_until_ = seconds_ + help_at_start;
}

void GameView::show_help() noexcept {
    help_until_ = std::max(help_until_, seconds_ + help_after_change);
}

void GameView::handle(std::span<const GameEvent> events, const ClientWorld& world,
                      const Preferences& preferences, Clock::time_point now) {
    effects_.set_settings(preferences.effects);
    for (const GameEvent& event : events) {
        // The sounds listen to the bus (GameSounds).
        std::visit([this](const auto& any) { events_.get().publish(any); }, event);
        if (const auto* const spawned = std::get_if<SpawnEvent>(&event)) {
            kinds_[spawned->id] = spawned->kind;
        } else if (const auto* const destroyed = std::get_if<DestroyEvent>(&event)) {
            const auto found = kinds_.find(destroyed->id);
            const bool ship = found != kinds_.end() &&
                              found->second == static_cast<std::uint8_t>(EntityKind::PlayerShip);
            if (found != kinds_.end()) {
                kinds_.erase(found);
            }
            // A ship's death has its own event, with its explosion.
            if (destroyed->reason == DestroyReason::Killed && !ship) {
                explode({destroyed->x, destroyed->y});
            }
        } else if (const auto* const death = std::get_if<DeathEvent>(&event)) {
            explode({death->x, death->y});
            if (death->player == player_) {
                constexpr float flash_alpha = 0.45F;
                constexpr float flash_seconds = 0.3F;
                constexpr float shake_strength = 10.0F;
                constexpr float shake_seconds = 0.4F;
                effects_.flash(render::rgb(0xFF2020, flash_alpha), flash_seconds);
                effects_.shake(shake_strength, shake_seconds);
            }
        }
        notices_.handle(event, now);
    }
    notices_.remember(world.players());
    notices_.prune(now);
}

void GameView::update(float seconds, const Preferences& preferences) {
    effects_.set_settings(preferences.effects);
    effects_.update(seconds);
    stars_.update(static_cast<double>(seconds));
    seconds_ += static_cast<double>(seconds);
    const ecs::Tick tick{
        .step = std::chrono::duration_cast<core::Duration>(std::chrono::duration<float>{seconds}),
        .index = blast_ticks_++};
    ecs::animate_sprites(blasts_, tick);
    ecs::destroy_finished_animations(blasts_, faded_);
}

render::Color GameView::background(const Preferences& preferences) const {
    return effects_.tint(color(background_color, preferences.vision));
}

bool GameView::game_over(const ClientWorld& world) {
    bool any = false;
    for (const std::optional<RemotePlayer>& player : world.players()) {
        if (!player.has_value()) {
            continue;
        }
        any = true;
        if (player->ship != no_entity || player->lives > 0) {
            return false;
        }
    }
    return any;
}

void GameView::draw(render::IRenderer& renderer, ClientWorld& world,
                    const ShipPrediction& prediction, const Preferences& preferences,
                    Clock::time_point now) {
    stars_.draw(renderer);
    draw_world(renderer, world, prediction, preferences.vision, now);
    draw_hud(renderer, world, preferences);
    draw_messages(renderer, world, preferences);
    if (const std::optional<render::Sprite> flash = effects_.overlay({width, height})) {
        renderer.draw(*flash);
    }
}

void GameView::explode(Vec2 position) {
    const GameArt& art = art_.get();
    const ecs::Entity blast = blasts_.create();
    blasts_.emplace<ecs::Transform>(blast, position);
    blasts_.emplace<ecs::Sprite>(blast,
                                 ecs::Sprite{.size = art.frame_size(Art::Explosion) * art_scale});
    blasts_.emplace<ecs::Animation>(blast, ecs::Animation{.count = art.frames(Art::Explosion),
                                                          .frame_time = explosion_frame_time,
                                                          .loop = false});
    blasts_.emplace<ecs::DestroyAfterAnimation>(blast);
}

void GameView::draw_world(render::IRenderer& renderer, ClientWorld& world,
                          const ShipPrediction& prediction, std::size_t vision,
                          Clock::time_point now) {
    const GameArt& art = art_.get();
    const render::BitmapFont& font = font_.get();
    const Vec2 offset = effects_.offset();
    const auto sheet = [&](Art kind, Vec2 at, std::uint32_t frame, render::Color tint) {
        const Vec2 size = art.frame_size(kind) * art_scale;
        renderer.draw({.texture = art.texture(kind, vision),
                       .position = at - (size / 2.0F),
                       .size = size,
                       .source = art.source(kind, frame),
                       .tint = tint,
                       .layer = world_layer});
    };
    // Every entity drawn interpolation_delay in the past, between two snapshots (#127), but
    // the player's own ship, drawn where its inputs put it.
    const std::optional<Vec2> predicted = prediction.drawn();
    if (const std::optional<double> tick = world.render_tick(now)) {
        world.registry().view<Replicated, Trail>().each([&](const Replicated& replicated,
                                                            const Trail& trail) {
            const bool own = predicted.has_value() && prediction.ship() == replicated.id;
            // Spawned after the tick drawn: shown once the drawing reaches it.
            if (!own &&
                (trail.count == 0 || static_cast<double>(trail.points.front().tick) > *tick)) {
                return;
            }
            const Vec2 at = (own ? predicted.value_or(Vec2{}) : interpolate(trail, *tick)) + offset;
            switch (static_cast<EntityKind>(replicated.kind)) {
            case EntityKind::PlayerShip: {
                const std::size_t owner = replicated.owner % render::max_players;
                const render::Color tint = player_color(owner, vision);
                sheet(Art::Ship, at, frame_at(seconds_, ship_frame_time, replicated.id), tint);
                // The slot's marker and number behind the ship: its player at a glance,
                // with or without color.
                const float half = art.frame_size(Art::Ship).x * art_scale / 2.0F;
                const Vec2 marker_at{at.x - half - marker_size - 6.0F, at.y - (marker_size / 2.0F)};
                renderer.draw({.texture = art.marker(owner),
                               .position = marker_at,
                               .size = {marker_size, marker_size},
                               .tint = tint,
                               .layer = hud_layer - 2});
                const std::string label = owner == player_ ? std::format("P{} YOU", owner + 1)
                                                           : std::format("P{}", owner + 1);
                // Above the marker, or under it at the top of the screen.
                const float above = marker_at.y - font.line_height(2.0F) - 4.0F;
                const float label_y = above >= 0.0F ? above : marker_at.y + marker_size + 4.0F;
                font.draw(renderer, label, {marker_at.x, label_y},
                          {.scale = 2.0F, .color = tint, .layer = hud_layer - 2});
                break;
            }
            case EntityKind::BydosSlave:
                sheet(Art::Bydos, at, frame_at(seconds_, bydos_frame_time, replicated.id),
                      color(text_color, vision));
                break;
            case EntityKind::PlayerMissile: {
                const Vec2 size{20.0F, 4.0F};
                renderer.draw({.position = at - (size / 2.0F),
                               .size = size,
                               .tint = player_color(replicated.owner % render::max_players, vision),
                               .layer = world_layer + 1});
                break;
            }
            case EntityKind::EnemyMissile: {
                const Vec2 size{10.0F, 10.0F};
                renderer.draw({.position = at - (size / 2.0F),
                               .size = size,
                               .tint = color(enemy_missile_color, vision),
                               .layer = world_layer + 1});
                break;
            }
            }
        });
    }
    blasts_.view<ecs::Transform, ecs::Sprite>().each(
        [&](const ecs::Transform& transform, const ecs::Sprite& sprite) {
            renderer.draw({.texture = art.texture(Art::Explosion, vision),
                           .position = transform.position + offset - (sprite.size / 2.0F),
                           .size = sprite.size,
                           .source = art.source(Art::Explosion, sprite.frame),
                           .layer = world_layer + 2});
        });
}

void GameView::draw_hud(render::IRenderer& renderer, const ClientWorld& world,
                        const Preferences& preferences) const {
    const GameArt& art = art_.get();
    const render::BitmapFont& font = font_.get();
    const std::size_t vision = preferences.vision;
    renderer.draw({.position = {0.0F, height - hud_height},
                   .size = {width, hud_height},
                   .tint = faded(color(0x000000, vision), 0.75F),
                   .layer = hud_layer - 1});
    const float panel = width / static_cast<float>(tuning::max_players);
    // Two lines per slot; one text size for every slot, the player's own, as large as the
    // widest slot allows.
    std::array<std::array<std::string, 2>, tuning::max_players> lines;
    float widest = 1.0F;
    for (std::size_t slot = 0; slot < tuning::max_players; ++slot) {
        const std::optional<RemotePlayer>& player = world.players().at(slot);
        auto& [first, second] = lines.at(slot);
        first = std::format("P{} ---", slot + 1);
        if (player.has_value()) {
            const auto number = static_cast<std::uint8_t>(slot);
            // A long name is cut, so that the slots keep a readable size.
            std::string name = name_of(world, number);
            if (name.size() > hud_name_length) {
                name = name.substr(0, hud_name_length - 1) + ".";
            }
            first = std::format("P{} {}", slot + 1, name);
            second = player->ship == no_entity && player->lives == 0
                         ? std::format("OUT  {:06}", player->score)
                         : std::format("LIVES {}  {:06}", player->lives, player->score);
        }
        widest = std::max({widest, font.measure(first).x, font.measure(second).x});
    }
    const float room = panel - marker_size - (2.0F * margin) - 8.0F;
    const float wanted = preferences.text_size.apply({.scale = 2.0F}).scale;
    const float tallest = (hud_height - 16.0F) / ((2.0F * font.line_height(1.0F)) + 4.0F);
    const float scale = std::max(1.0F, std::min({room / widest, tallest, wanted}));
    for (std::size_t slot = 0; slot < tuning::max_players; ++slot) {
        const render::Color tint = world.players().at(slot).has_value() ? player_color(slot, vision)
                                                                        : color(dim_color, vision);
        const Vec2 at{(panel * static_cast<float>(slot)) + margin, height - hud_height + 10.0F};
        // The player's own slot stands out.
        if (slot == player_) {
            renderer.draw({.position = {panel * static_cast<float>(slot), height - hud_height},
                           .size = {panel, hud_height},
                           .tint = faded(player_color(slot, vision), 0.18F),
                           .layer = hud_layer - 1});
        }
        renderer.draw({.texture = art.marker(slot),
                       .position = at,
                       .size = {marker_size, marker_size},
                       .tint = tint,
                       .layer = hud_layer});
        const render::TextStyle style{.scale = scale, .color = tint, .layer = hud_layer};
        const float text_x = at.x + marker_size + 8.0F;
        font.draw(renderer, lines.at(slot).at(0), {text_x, at.y}, style);
        font.draw(renderer, lines.at(slot).at(1), {text_x, at.y + font.line_height(scale) + 4.0F},
                  style);
    }
}

void GameView::draw_messages(render::IRenderer& renderer, const ClientWorld& world,
                             const Preferences& preferences) const {
    const render::BitmapFont& font = font_.get();
    const std::size_t vision = preferences.vision;
    // The players who joined or left (#129), at the top, in their slot's color.
    const render::TextStyle notice_style =
        preferences.text_size.apply({.scale = 2.0F, .layer = hud_layer});
    float y = 48.0F;
    for (const PlayerNotice& notice : notices_.shown()) {
        const std::string text = upper(notice.text);
        const Vec2 size = font.measure(text, notice_style.scale);
        const Vec2 at{(width - size.x) / 2.0F, y};
        renderer.draw({.position = at - Vec2{10.0F, 6.0F},
                       .size = size + Vec2{20.0F, 12.0F},
                       .tint = faded(color(0x000000, vision), 0.6F),
                       .layer = hud_layer - 1});
        render::TextStyle style = notice_style;
        style.color = player_color(notice.player, vision);
        font.draw(renderer, text, at, style);
        y += size.y + 18.0F;
    }
    // The keys of the game, at the top left, for a few seconds.
    if (seconds_ < help_until_) {
        draw_help(renderer, preferences);
    }

    std::string banner;
    if (game_over(world)) {
        banner = "GAME OVER";
    } else if (const std::optional<RemotePlayer>& self =
                   world.players().at(player_ % tuning::max_players);
               self.has_value() && self->ship == no_entity && self->lives == 0) {
        banner = "YOU ARE OUT";
    }
    if (!banner.empty()) {
        const render::TextStyle style = preferences.text_size.apply(
            {.scale = 5.0F, .color = color(text_color, vision), .layer = hud_layer});
        const Vec2 size = font.measure(banner, style.scale);
        font.draw(renderer, banner, Vec2{width - size.x, height - size.y - hud_height} / 2.0F,
                  style);
    }
}

void GameView::draw_help(render::IRenderer& renderer, const Preferences& preferences) const {
    const render::BitmapFont& font = font_.get();
    const std::string help =
        std::format("ESC LEAVE  F1 AUTO-FIRE {}  F2 EFFECTS {}  F3 {}  F4/F5 TEXT  F6 LAG  M MUSIC",
                    preferences.controls.auto_fire().enabled() ? "ON" : "OFF",
                    preferences.effects.flashes || preferences.effects.shake ? "ON" : "OFF",
                    upper(std::string{vision_names.at(preferences.vision % vision_count)}));
    // Whole scales only: the pixel font stays sharp.
    const float help_scale = std::max(
        1.0F, std::floor(std::min(2.0F, (width - (2.0F * margin)) / font.measure(help).x)));
    font.draw(
        renderer, help, {margin, 12.0F},
        {.scale = help_scale, .color = color(dim_color, preferences.vision), .layer = hud_layer});
}

std::string GameView::name_of(const ClientWorld& world, std::uint8_t player) const {
    if (player == player_) {
        return name_;
    }
    const std::optional<RemotePlayer>& remote = world.players().at(player);
    return remote.has_value() ? remote->name : std::string{};
}

} // namespace rtype
