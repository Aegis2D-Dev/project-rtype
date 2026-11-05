/// @file
/// @brief `r-type_client`: the window, the screens and the session with the server.
#pragma once
#include <aegis2d/core/event_bus.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/platform/actions.hpp>
#include <aegis2d/platform/key_capture.hpp>
#include <aegis2d/platform/window.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/opengl/renderer.hpp>
#include <aegis2d/render/texture_manager.hpp>

#include "game_art.hpp"
#include "game_sounds.hpp"
#include "game_view.hpp"
#include "menu_screens.hpp"
#include "preferences.hpp"
#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/connect_fields.hpp>
#include <rtype/connect_form.hpp>
#include <rtype/lagometer.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace rtype {

/// @brief The screen the client shows.
enum class ClientScreen : std::uint8_t {
    /// @brief The connect screen: server, port, name and controls (#123).
    Connect,
    /// @brief Joining the server: login, then UDP binding.
    Joining,
    /// @brief In game.
    Game,
    /// @brief The server stopped or went silent (#128).
    Lost,
};

/// @brief The keys that change the player's settings, on every screen.
struct OptionKeys {
    /// @brief F1 (or the gamepad's Back): auto-fire on or off.
    aegis2d::platform::ActionId auto_fire = 0;
    /// @brief F2: flashes and shake on or off.
    aegis2d::platform::ActionId effects = 0;
    /// @brief F3: next color vision.
    aegis2d::platform::ActionId vision = 0;
    /// @brief F4: smaller text.
    aegis2d::platform::ActionId smaller = 0;
    /// @brief F5: larger text.
    aegis2d::platform::ActionId larger = 0;
    /// @brief F6: lagometer on or off.
    aegis2d::platform::ActionId lagometer = 0;
    /// @brief M, in game: music on or off.
    aegis2d::platform::ActionId music = 0;
};

/// @brief The graphical client: one window, the screens of the game, and the session.
/// @details Each frame reads the keyboard and the gamepad, runs the network handlers that
/// are ready (NetworkContext::poll()) and the session's timers, then draws the screen.
/// In game, the held actions go to ClientSession::set_input(), sent 60 times a second,
/// which also tells the server the client is alive; the server's snapshots and events
/// update ClientWorld, drawn by GameView; F6 shows the lagometer over it. When the server stops,
/// closes the connection or sends nothing for ClientSession::server_timeout, the Lost screen says
/// so; Enter goes back to the connect screen, filled as before, to join again.
class ClientApp {
public:
    /// @brief Opens the window and loads the art, sounds and the player's settings.
    /// @param options Command line: server, name, `--join`, settings file, frames.
    /// @throws std::exception if the window, the renderer or an asset cannot be opened.
    explicit ClientApp(const ClientOptions& options);

    /// @brief Runs until the window closes, Escape on the connect screen, or `--frames`.
    /// @details Leaves the server first if in game.
    void run();

private:
    [[nodiscard]] bool frame();
    void read_options_keys();
    void connect_screen();
    void joining_screen();
    void game_screen();
    void lost_screen();
    void join(const ClientSettings& settings);
    void leave();
    void update_session();
    void draw();
    void select(std::size_t row);

    ClientOptions options_;
    Preferences preferences_;
    aegis2d::platform::Window window_;
    aegis2d::render::opengl::Renderer renderer_;
    aegis2d::render::TextureManager textures_;
    aegis2d::render::BitmapFont font_;
    GameArt art_;
    aegis2d::core::EventBus events_;
    GameSounds sounds_;
    GameView view_;
    aegis2d::platform::ActionMap menu_;
    OptionKeys keys_;
    aegis2d::platform::KeyCapture capture_;
    aegis2d::net::NetworkContext context_;
    std::optional<ClientSession> session_;
    Lagometer lagometer_;
    bool show_lagometer_ = false;
    ClientSettings joined_;
    ConnectFields fields_;
    ScreenMessage message_;
    std::string lost_reason_;
    ClientScreen screen_ = ClientScreen::Connect;
    std::size_t row_ = 0;
    bool quit_ = false;
    std::uint64_t frames_ = 0;
    std::chrono::steady_clock::time_point last_frame_;
    float frame_seconds_ = 0.0F;
};

} // namespace rtype
