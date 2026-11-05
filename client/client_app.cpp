#include "client_app.hpp"

#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/log.hpp>
#include <aegis2d/core/loop.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/platform/actions.hpp>
#include <aegis2d/platform/controls.hpp>
#include <aegis2d/platform/gamepad.hpp>
#include <aegis2d/platform/key_capture.hpp>
#include <aegis2d/platform/keyboard.hpp>
#include <aegis2d/platform/window.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/letterbox.hpp>
#include <aegis2d/render/opengl/renderer.hpp>

#include "game_art.hpp"
#include "game_sounds.hpp"
#include "game_view.hpp"
#include "lagometer_view.hpp"
#include "menu_screens.hpp"
#include "preferences.hpp"
#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/connect_fields.hpp>
#include <rtype/connect_form.hpp>
#include <rtype/events.hpp>
#include <rtype/login.hpp>
#include <rtype/tuning.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rtype {

namespace core = aegis2d::core;
namespace platform = aegis2d::platform;
namespace render = aegis2d::render;
using platform::Key;

namespace {

using Clock = std::chrono::steady_clock;

// How long the network runs after leaving, for the Logout and Disconnect to leave.
constexpr std::chrono::milliseconds goodbye{200};
constexpr std::chrono::milliseconds goodbye_slice{10};
// A frame never moves the game more than this, after a stall (a window dragged).
constexpr float longest_frame = 0.1F;

// The session's reasons are lower case, for logs: a capital and a full stop for the screen.
std::string sentence(std::string text) {
    if (!text.empty() && text.front() >= 'a' && text.front() <= 'z') {
        text.front() = static_cast<char>(text.front() - 'a' + 'A');
    }
    if (!text.empty() && text.back() != '.') {
        text += '.';
    }
    return text;
}

bool shift(const platform::Keyboard& keyboard) {
    return keyboard.held(Key::LeftShift) || keyboard.held(Key::RightShift);
}

} // namespace

ClientApp::ClientApp(const ClientOptions& options)
    : options_{options},
      preferences_{load_preferences(options.settings_file.value_or(default_preferences_file()))},
      window_{{.title = "R-Type",
               .width = static_cast<int>(tuning::area_width),
               .height = static_cast<int>(tuning::area_height),
               .resizable = true}},
      renderer_{window_}, textures_{renderer_}, font_{render::BitmapFont::builtin(textures_)},
      art_{textures_}, sounds_{events_}, view_{art_, font_, events_}, fields_{options.settings},
      last_frame_{Clock::now()} {
    // The game is laid out on a 1280 x 720 screen, scaled to the window with black bars.
    renderer_.set_logical_screen(
        render::LogicalScreen{.size = {tuning::area_width, tuning::area_height}});
    keys_ = OptionKeys{
        .auto_fire = menu_.add("toggle-auto-fire", {Key::F1}, {platform::GamepadButton::Back}),
        .effects = menu_.add("toggle-effects", {Key::F2}),
        .vision = menu_.add("color-vision", {Key::F3}),
        .smaller = menu_.add("text-smaller", {Key::F4}),
        .larger = menu_.add("text-larger", {Key::F5}),
        .lagometer = menu_.add("toggle-lagometer", {Key::F6}),
        .music = menu_.add("toggle-music", {Key::M}),
    };
    core::log_info("settings: {}", preferences_.file.string());
    select(static_cast<std::size_t>(ConnectField::Name));
    if (options.join) {
        join(options.settings);
    }
}

void ClientApp::run() {
    core::MainLoop loop{core::FixedTimestep{core::step_from_rate(tuning::tick_rate)}};
    loop.set_render([this, &loop](double /*alpha*/) {
        if (!frame()) {
            loop.stop();
        }
    });
    loop.run();
    leave();
}

bool ClientApp::frame() {
    window_.poll_events();
    if (window_.close_requested() || quit_ || (options_.frames > 0 && frames_ == options_.frames)) {
        return false;
    }
    const Clock::time_point now = Clock::now();
    frame_seconds_ =
        std::min(std::chrono::duration<float>{now - last_frame_}.count(), longest_frame);
    last_frame_ = now;

    if (!capture_.waiting().has_value()) {
        read_options_keys();
    }
    switch (screen_) {
    case ClientScreen::Connect:
        connect_screen();
        break;
    case ClientScreen::Joining:
        joining_screen();
        break;
    case ClientScreen::Game:
        game_screen();
        break;
    case ClientScreen::Lost:
        lost_screen();
        break;
    }
    // The fields take typed text while one is selected on the connect screen; the game's
    // keys are actions then, never text.
    window_.set_text_input(screen_ == ClientScreen::Connect && !capture_.waiting().has_value() &&
                           fields_.focused().has_value());
    update_session();
    sounds_.update();
    draw();
    ++frames_;
    return true;
}

void ClientApp::read_options_keys() {
    const platform::Keyboard& keyboard = window_.keyboard();
    const platform::Gamepad& gamepad = window_.gamepad();
    bool changed = false;
    if (menu_.pressed(keys_.auto_fire, keyboard, gamepad)) {
        platform::LatchedAction& auto_fire = preferences_.controls.auto_fire();
        auto_fire.set_enabled(!auto_fire.enabled());
        core::log_info("auto-fire {}", auto_fire.enabled() ? "on" : "off");
        changed = true;
    }
    if (menu_.pressed(keys_.effects, keyboard)) {
        const bool on = !(preferences_.effects.flashes || preferences_.effects.shake);
        preferences_.effects.flashes = on;
        preferences_.effects.shake = on;
        core::log_info("flashes and shake {}", on ? "on" : "off");
        changed = true;
    }
    if (menu_.pressed(keys_.vision, keyboard)) {
        preferences_.vision = (preferences_.vision + 1) % vision_count;
        core::log_info("color vision: {}", vision_names.at(preferences_.vision));
    }
    if (menu_.pressed(keys_.smaller, keyboard)) {
        preferences_.text_size.smaller();
        changed = true;
    }
    if (menu_.pressed(keys_.larger, keyboard)) {
        preferences_.text_size.larger();
        changed = true;
    }
    if (menu_.pressed(keys_.lagometer, keyboard)) {
        show_lagometer_ = !show_lagometer_;
        core::log_info("lagometer {}", show_lagometer_ ? "on" : "off");
    }
    if (changed) {
        save_preferences(preferences_);
    }
    if (screen_ == ClientScreen::Game && (changed || menu_.pressed(keys_.vision, keyboard) ||
                                          menu_.pressed(keys_.music, keyboard))) {
        view_.show_help();
    }
}

void ClientApp::connect_screen() {
    const platform::Keyboard& keyboard = window_.keyboard();
    platform::ActionMap& actions = preferences_.controls.actions();
    // A control waiting for its new key takes the next key pressed (Escape cancels).
    if (capture_.waiting().has_value()) {
        if (const std::optional<platform::CapturedKey> captured =
                capture_.update(actions, keyboard)) {
            const std::string name = platform::action_label(actions.name(captured->action));
            if (captured->key.has_value()) {
                save_preferences(preferences_);
                message_ = {.text = std::format("{}: {}", name,
                                                platform::keys_label(actions, captured->action)),
                            .error = false};
                if (captured->taken_from.has_value()) {
                    message_.text +=
                        std::format(" (taken from {})",
                                    platform::action_label(actions.name(*captured->taken_from)));
                }
            } else {
                message_ = {.text = std::format("{}: key unchanged", name), .error = false};
            }
        }
        return;
    }
    // The frame's typed text and editing keys first, in the field selected when they were
    // pressed: a character typed just before Tab stays in its field, one typed just before
    // Enter is joined with.
    fields_.update(window_.text_input(), keyboard);
    for (const Key key : keyboard.pressed_keys()) {
        switch (key) {
        case Key::Escape:
            quit_ = true;
            return;
        case Key::Tab:
            select(shift(keyboard) ? row_ + connect_rows - 1 : row_ + 1);
            break;
        case Key::Up:
            select(row_ + connect_rows - 1);
            break;
        case Key::Down:
            select(row_ + 1);
            break;
        case Key::Enter:
        case Key::KeypadEnter:
            if (row_ >= connect_field_count) {
                capture_.start(
                    preferences_.controls.id(control_rows.at(row_ - connect_field_count)),
                    keyboard);
                return;
            }
            if (const auto settings = fields_.form().submit();
                std::holds_alternative<ClientSettings>(settings)) {
                join(std::get<ClientSettings>(settings));
            } else {
                message_ = {.text = std::get<std::string>(settings), .error = true};
            }
            return;
        default:
            break;
        }
    }
}

void ClientApp::joining_screen() {
    if (window_.keyboard().pressed(Key::Escape)) {
        leave();
        message_ = {.text = "Joining cancelled.", .error = false};
        screen_ = ClientScreen::Connect;
    }
}

void ClientApp::game_screen() {
    const platform::Keyboard& keyboard = window_.keyboard();
    const platform::Gamepad& gamepad = window_.gamepad();
    if (keyboard.pressed(Key::Escape)) {
        leave();
        message_ = {.text = "You left the game.", .error = false};
        screen_ = ClientScreen::Connect;
        return;
    }
    if (menu_.pressed(keys_.music, keyboard)) {
        sounds_.toggle_music();
        core::log_info("music {}", sounds_.music_playing() ? "on" : "off");
    }
    // What the player holds, sent 60 times a second by the session.
    aegis2d::ecs::PlayerInput input;
    input.held = preferences_.controls.update(keyboard, gamepad);
    if (session_.has_value()) {
        session_->set_input(input);
    }
}

void ClientApp::lost_screen() {
    const platform::Keyboard& keyboard = window_.keyboard();
    if (keyboard.pressed(Key::Enter) || keyboard.pressed(Key::KeypadEnter)) {
        message_ = {.text = sentence(lost_reason_), .error = true};
        screen_ = ClientScreen::Connect;
    } else if (keyboard.pressed(Key::Escape)) {
        quit_ = true;
    }
}

void ClientApp::join(const ClientSettings& settings) {
    message_ = {};
    joined_ = settings;
    try {
        session_.emplace(context_, settings);
        lagometer_.clear();
        session_->start();
        screen_ = ClientScreen::Joining;
        core::log_info("joining {}:{} as {}", settings.host, settings.tcp_port, settings.name);
    } catch (const std::exception& error) {
        session_.reset();
        message_ = {.text = error.what(), .error = true};
        screen_ = ClientScreen::Connect;
    }
}

void ClientApp::leave() {
    if (!session_.has_value()) {
        return;
    }
    const bool said_goodbye =
        session_->state() == ClientState::InGame || session_->state() == ClientState::Binding;
    session_->leave();
    if (said_goodbye) {
        // Keeps the network running a moment, for the Logout to leave.
        const Clock::time_point end = Clock::now() + goodbye;
        while (Clock::now() < end) {
            context_.run_for(goodbye_slice);
            session_->update(Clock::now());
        }
        core::log_info("left the game");
    }
    session_.reset();
}

void ClientApp::update_session() {
    if (!session_.has_value()) {
        return;
    }
    context_.poll();
    const Clock::time_point now = Clock::now();
    session_->update(now);
    switch (session_->state()) {
    case ClientState::InGame:
        if (screen_ == ClientScreen::Joining) {
            const std::uint8_t player = session_->login().value_or(LoginAccepted{}).player;
            view_.start(player, joined_.name);
            screen_ = ClientScreen::Game;
            core::log_info("in game as player {}", player + 1);
        }
        lagometer_.update(session_->rtt(), session_->traffic(), session_->network_stats(), now,
                          session_->prediction().corrections());
        if (screen_ == ClientScreen::Game) {
            const std::vector<GameEvent> events = session_->world().take_events();
            view_.handle(events, session_->world(), preferences_, now);
            view_.update(frame_seconds_, preferences_);
        }
        break;
    case ClientState::Failed:
        core::log_warn("could not join: {}", session_->error());
        message_ = {.text = sentence(session_->error()), .error = true};
        session_.reset();
        screen_ = ClientScreen::Connect;
        break;
    case ClientState::Lost:
        // #128: the server stopped, closed the connection or went silent.
        core::log_warn("connection lost: {}", session_->error());
        lost_reason_ = session_->error();
        session_.reset();
        screen_ = ClientScreen::Lost;
        break;
    case ClientState::Closed:
        session_.reset();
        screen_ = ClientScreen::Connect;
        break;
    case ClientState::Idle:
    case ClientState::Connecting:
    case ClientState::LoggingIn:
    case ClientState::Binding:
        break;
    }
}

void ClientApp::draw() {
    const bool capture = options_.screenshot.has_value() && frames_ + 1 == options_.frames;
    if (capture) {
        renderer_.capture_next_frame();
    }
    const Clock::time_point now = Clock::now();
    if (screen_ == ClientScreen::Game && session_.has_value()) {
        renderer_.begin_frame(view_.background(preferences_));
        view_.draw(renderer_, session_->world(), session_->prediction(), preferences_, now);
        if (show_lagometer_) {
            draw_lagometer(renderer_, font_, lagometer_, preferences_);
        }
    } else {
        renderer_.begin_frame(seen_as(render::rgb(0x0A0A1A), preferences_.vision));
        switch (screen_) {
        case ClientScreen::Joining:
            draw_status(
                renderer_, font_, "JOINING",
                std::format("{}:{} as {}, {}...", joined_.host, joined_.tcp_port, joined_.name,
                            session_.has_value() ? to_string(session_->state()) : "connecting"),
                "ESCAPE: CANCEL", preferences_);
            break;
        case ClientScreen::Lost:
            draw_status(renderer_, font_, "CONNECTION LOST", sentence(lost_reason_),
                        "ENTER: BACK TO THE MENU    ESCAPE: QUIT", preferences_);
            break;
        case ClientScreen::Connect:
        case ClientScreen::Game:
            draw_connect(renderer_, font_, fields_, message_,
                         ConnectView{.row = row_, .capturing = capture_.waiting().has_value()},
                         preferences_);
            break;
        }
    }
    renderer_.end_frame();
    if (capture && renderer_.captured().has_value()) {
        renderer_.captured()->save_png(*options_.screenshot);
        core::log_info("screenshot saved to {}", options_.screenshot->string());
    }
}

void ClientApp::select(std::size_t row) {
    row_ = row % connect_rows;
    fields_.focus(row_ < connect_field_count ? std::optional{static_cast<ConnectField>(row_)}
                                             : std::nullopt);
}

} // namespace rtype
