/// @file
/// @brief The game screen: the world the server describes, its effects and the HUD.
#pragma once
#include <aegis2d/core/event_bus.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/renderer.hpp>
#include <aegis2d/render/screen_effects.hpp>
#include <aegis2d/render/starfield.hpp>

#include "game_art.hpp"
#include "preferences.hpp"
#include <rtype/client_world.hpp>
#include <rtype/events.hpp>
#include <rtype/player_notices.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype {

/// @brief Draws a game in progress, from the client's copy of the world.
/// @details Each frame, the client hands the events the world applied (handle()), moves
/// time forward (update()) and draws (draw()). The remote entities are drawn where they
/// were ClientWorld::interpolation_delay() ago, between two snapshots (#127): smooth at any
/// frame rate though the server sends 20 snapshots a second. On top of the world:
/// - each ship in its player's color, with the marker and number of its slot, so that
///   players tell each other apart without color too;
/// - explosions where an enemy or a ship was killed, a red flash and a shake when the
///   player's own ship is hit (turned off by the player's effects setting);
/// - the sounds, through the events published on the bus (GameSounds);
/// - the HUD: name, lives and score of each player;
/// - the notices of players joining or leaving (#129), and "game over".
class GameView {
public:
    /// @brief Clock of the frames.
    using Clock = std::chrono::steady_clock;

    /// @brief Creates the view.
    /// @param art Sprite sheets and markers; must outlive this object.
    /// @param font Font of the HUD; must outlive this object.
    /// @param events Bus the events are published on, for the sounds; must outlive this
    /// object.
    GameView(const GameArt& art, const aegis2d::render::BitmapFont& font,
             aegis2d::core::EventBus& events);

    /// @brief Starts a new game: forgets the explosions and notices of the last one.
    /// @param player This client's player number.
    /// @param name This client's player name, which the server never sends back to it.
    void start(std::uint8_t player, std::string name);

    /// @brief Shows the events the world applied this frame: explosions, sounds, notices.
    /// @param events Events from ClientWorld::take_events(), in order.
    /// @param world The world after the events.
    /// @param preferences The player's settings, for the effects.
    /// @param now Time of the frame.
    void handle(std::span<const GameEvent> events, const ClientWorld& world,
                const Preferences& preferences, Clock::time_point now);

    /// @brief Moves the stars, explosions and effects forward.
    /// @param seconds Time since the last frame.
    /// @param preferences The player's settings, for the effects.
    void update(float seconds, const Preferences& preferences);

    /// @brief Draws the frame.
    /// @param renderer Renderer, between begin_frame() and end_frame().
    /// @param world The world to draw.
    /// @param preferences The player's settings: text size, color vision, auto-fire.
    /// @param now Time of the frame.
    void draw(aegis2d::render::IRenderer& renderer, ClientWorld& world,
              const Preferences& preferences, Clock::time_point now);

    /// @brief Shows the keys of the game at the top for a few seconds (they also show for
    /// the first seconds of a game), after the player changed a setting.
    void show_help() noexcept;

    /// @brief Background color of the frame, with the flash blended in.
    /// @param preferences The player's settings, for the color vision.
    /// @return The color to clear the screen with.
    [[nodiscard]] aegis2d::render::Color background(const Preferences& preferences) const;

    /// @brief Whether every player of the game is out of lives.
    /// @param world The world.
    /// @return `true` once the last ship is gone.
    [[nodiscard]] static bool game_over(const ClientWorld& world);

private:
    void explode(aegis2d::Vec2 position);
    void draw_world(aegis2d::render::IRenderer& renderer, ClientWorld& world, std::size_t vision,
                    Clock::time_point now);
    void draw_hud(aegis2d::render::IRenderer& renderer, const ClientWorld& world,
                  const Preferences& preferences) const;
    void draw_messages(aegis2d::render::IRenderer& renderer, const ClientWorld& world,
                       const Preferences& preferences) const;
    void draw_help(aegis2d::render::IRenderer& renderer, const Preferences& preferences) const;
    [[nodiscard]] std::string name_of(const ClientWorld& world, std::uint8_t player) const;

    std::reference_wrapper<const GameArt> art_;
    std::reference_wrapper<const aegis2d::render::BitmapFont> font_;
    std::reference_wrapper<aegis2d::core::EventBus> events_;
    aegis2d::render::Starfield stars_;
    aegis2d::render::ScreenEffects effects_;
    // Explosions: local entities of the client, gone after their last frame.
    aegis2d::ecs::Registry blasts_;
    std::vector<aegis2d::ecs::Entity> faded_;
    std::uint64_t blast_ticks_ = 0;
    PlayerNotices notices_;
    // Kind of each entity of the world, by network id, to know what a Destroy removed.
    std::unordered_map<std::uint32_t, std::uint8_t> kinds_;
    std::uint8_t player_ = 0;
    std::string name_;
    double seconds_ = 0.0;
    double help_until_ = 0.0;
};

} // namespace rtype
