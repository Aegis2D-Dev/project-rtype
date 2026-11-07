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
#include <aegis2d/ui/navigation.hpp>

#include "editor_view.hpp"
#include "game_art.hpp"
#include "game_sounds.hpp"
#include "game_view.hpp"
#include "lagometer_view.hpp"
#include "menu_screens.hpp"
#include "preferences.hpp"
#include "typed_character.hpp"
#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/connect_fields.hpp>
#include <rtype/connect_form.hpp>
#include <rtype/events.hpp>
#include <rtype/login.hpp>
#include <rtype/room_list_form.hpp>
#include <rtype/rooms.hpp>
#include <rtype/tuning.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rtype {

namespace core = aegis2d::core;
namespace platform = aegis2d::platform;
namespace render = aegis2d::render;
namespace ui = aegis2d::ui;
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
      started_{Clock::now()}, last_frame_{started_} {
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
        .metrics = menu_.add("toggle-metrics", {Key::F12}),
        .music = menu_.add("toggle-music", {Key::M}),
    };
    metrics_overlay_.set_visible(options.metrics);
    core::log_info("settings: {}", preferences_.file.string());
    select(static_cast<std::size_t>(ConnectField::Name));
    // --edit opens a waves file in the level editor, or says why it cannot.
    if (options.edit.has_value()) {
        try {
            editor_.emplace(LevelDraft::from_file(*options.edit));
            screen_ = ClientScreen::Editor;
            core::log_info("level editor: {}", options.edit->string());
        } catch (const core::ConfigError& error) {
            message_ = {.text = error.what(), .error = true};
        }
    }
    // --join joins with the fields as shown, so a rejoin from this screen is the same.
    if (options.join) {
        if (const auto settings = fields_.form().submit();
            std::holds_alternative<ClientSettings>(settings)) {
            join(std::get<ClientSettings>(settings));
        } else {
            message_ = {.text = std::get<std::string>(settings), .error = true};
        }
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
    metrics_.tick(now);
    frame_seconds_ =
        std::min(std::chrono::duration<float>{now - last_frame_}.count(), longest_frame);
    last_frame_ = now;

    if (!capture_.waiting().has_value()) {
        read_options_keys();
    }
    // While the chat line is open, the keys are its own.
    if (!chat_keys()) {
        switch (screen_) {
        case ClientScreen::Connect:
            connect_screen();
            break;
        case ClientScreen::Joining:
            joining_screen();
            break;
        case ClientScreen::Rooms:
            rooms_screen();
            break;
        case ClientScreen::Room:
            room_screen();
            break;
        case ClientScreen::Game:
            game_screen();
            break;
        case ClientScreen::Lost:
            lost_screen();
            break;
        case ClientScreen::Editor:
            editor_screen();
            break;
        case ClientScreen::Playtest:
            playtest_screen();
            break;
        }
    }
    // The fields take typed text while one is selected on the connect screen, and the chat
    // line while it is open; the game's keys are actions then, never text.
    window_.set_text_input(chatting_ ||
                           (screen_ == ClientScreen::Connect && !capture_.waiting().has_value() &&
                            fields_.focused().has_value()));
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
    if (menu_.pressed(keys_.metrics, keyboard)) {
        metrics_overlay_.toggle();
        core::log_info("frame metrics {}", metrics_overlay_.visible() ? "on" : "off");
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
                ClientSettings chosen = std::get<ClientSettings>(settings);
                chosen.choose_room = true;
                join(chosen);
            } else {
                message_ = {.text = std::get<std::string>(settings), .error = true};
            }
            return;
        default:
            break;
        }
    }
}

// The chat line (RFC section 3.8): T opens it in the lobby, in a room and in game, Enter
// sends what was typed and Escape closes it; while it is open, the keys are the line's. A room
// name being typed on the lobby screen takes the T.
bool ClientApp::chat_keys() {
    const bool can_chat =
        session_.has_value() && (screen_ == ClientScreen::Room || screen_ == ClientScreen::Game ||
                                 (screen_ == ClientScreen::Rooms && !room_list_.creating()));
    if (!can_chat) {
        chatting_ = false;
        return false;
    }
    const platform::Keyboard& keyboard = window_.keyboard();
    if (!chatting_) {
        if (!keyboard.pressed(Key::T)) {
            return false;
        }
        chatting_ = true;
        chat_field_.set_text({});
        chat_tree_.focus(chat_field_.id());
        // The ship stops while its player types.
        session_->set_input(aegis2d::ecs::PlayerInput{});
        return true;
    }
    if (keyboard.pressed(Key::Escape)) {
        chatting_ = false;
    } else if (keyboard.pressed(Key::Enter) || keyboard.pressed(Key::KeypadEnter)) {
        if (!chat_field_.text().empty()) {
            session_->send_chat(chat_field_.text());
        }
        chatting_ = false;
    } else {
        ui::Navigation navigation;
        if (keyboard.pressed(Key::Left)) {
            navigation.direction = ui::Direction::Left;
        } else if (keyboard.pressed(Key::Right)) {
            navigation.direction = ui::Direction::Right;
        }
        chat_field_.update(navigation, window_.text_input(), keyboard, chat_tree_);
    }
    return true;
}

void ClientApp::joining_screen() {
    if (window_.keyboard().pressed(Key::Escape)) {
        leave();
        message_ = {.text = "Joining cancelled.", .error = false};
        screen_ = ClientScreen::Connect;
    }
}

void ClientApp::rooms_screen() {
    const platform::Keyboard& keyboard = window_.keyboard();
    for (const Key key : keyboard.pressed_keys()) {
        switch (key) {
        case Key::Escape:
            leave();
            message_ = {.text = "You left the server.", .error = false};
            screen_ = ClientScreen::Connect;
            return;
        case Key::Up:
            room_list_.up();
            break;
        case Key::Down:
            room_list_.down();
            break;
        case Key::Left:
            room_list_.smaller();
            break;
        case Key::Right:
            room_list_.bigger();
            break;
        case Key::Backspace:
            room_list_.erase();
            break;
        case Key::Enter:
        case Key::KeypadEnter: {
            const auto choice = room_list_.submit();
            if (const auto* const room = std::get_if<std::uint8_t>(&choice);
                room != nullptr && session_.has_value()) {
                session_->join_room(*room);
                message_ = {.text = "Joining the room...", .error = false};
            } else if (const auto* const request = std::get_if<CreateRoomRequest>(&choice);
                       request != nullptr && session_.has_value()) {
                session_->create_room(*request);
                message_ = {.text = std::format("Room {} asked for.", request->name),
                            .error = false};
            } else if (const auto* const why = std::get_if<std::string>(&choice)) {
                message_ = {.text = *why, .error = true};
            }
            return;
        }
        default:
            if (const std::optional<char> character = typed_character(key, shift(keyboard))) {
                room_list_.type(*character);
            }
            break;
        }
    }
}

void ClientApp::room_screen() {
    const platform::Keyboard& keyboard = window_.keyboard();
    if (!session_.has_value()) {
        return;
    }
    if (keyboard.pressed(Key::Escape)) {
        session_->leave_room();
        message_ = {.text = "You left the room.", .error = false};
        screen_ = ClientScreen::Rooms;
    } else if (keyboard.pressed(Key::Enter) || keyboard.pressed(Key::KeypadEnter)) {
        session_->set_ready(!session_->ready());
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

// The level editor: the mouse places, moves and removes the enemies of the wave shown (#369).
void ClientApp::editor_screen() {
    const platform::Keyboard& keyboard = window_.keyboard();
    if (keyboard.pressed(Key::Escape) || !editor_.has_value()) {
        editor_.reset();
        screen_ = ClientScreen::Connect;
        return;
    }
    if (keyboard.pressed(Key::PageDown)) {
        editor_->next_wave();
    }
    if (keyboard.pressed(Key::PageUp)) {
        editor_->previous_wave();
    }
    // The wave in time (#370): Left and Right change its delay, Up and Down its order.
    if (keyboard.pressed(Key::Left)) {
        editor_->shift_delay(-0.1F);
    }
    if (keyboard.pressed(Key::Right)) {
        editor_->shift_delay(0.1F);
    }
    if (keyboard.pressed(Key::Up)) {
        editor_->move_wave(false);
    }
    if (keyboard.pressed(Key::Down)) {
        editor_->move_wave(true);
    }
    // N adds a wave, S saves the file, P plays the waves from the one edited (#371).
    if (keyboard.pressed(Key::N)) {
        editor_->add_wave();
    }
    const std::filesystem::path file = options_.edit.value_or("waves.json");
    if (keyboard.pressed(Key::S)) {
        try {
            editor_->draft().save(file);
            editor_->mark_saved();
            editor_message_ = {.text = std::format("Saved to {}.", file.string()), .error = false};
            core::log_info("level editor: saved {}", file.string());
        } catch (const core::ConfigError& error) {
            editor_message_ = {.text = error.what(), .error = true};
        }
    }
    if (keyboard.pressed(Key::P)) {
        try {
            auto game = std::make_unique<ServerGame>();
            game->play_waves(editor_->draft().schedule(editor_->wave()));
            playtest_ship_ = game->add_player(0);
            playtest_ = std::move(game);
            playtest_wave_ = editor_->wave();
            playtest_lag_ = 0.0F;
            editor_message_ = {};
            screen_ = ClientScreen::Playtest;
            return;
        } catch (const core::ConfigError& error) {
            editor_message_ = {.text = error.what(), .error = true};
        }
    }
    const platform::Mouse& mouse = window_.mouse();
    editor_->update({.position = renderer_.to_logical(mouse.position()),
                     .pressed = mouse.pressed(platform::MouseButton::Left),
                     .held = mouse.held(platform::MouseButton::Left),
                     .remove = mouse.pressed(platform::MouseButton::Right)},
                    editor_view_);
}

// The waves of the editor played on the client: the server's game, ticked here (#371).
void ClientApp::playtest_screen() {
    const platform::Keyboard& keyboard = window_.keyboard();
    if (keyboard.pressed(Key::Escape) || !playtest_) {
        playtest_.reset();
        screen_ = ClientScreen::Editor;
        return;
    }
    aegis2d::ecs::PlayerInput input;
    input.held = preferences_.controls.update(keyboard, window_.gamepad());
    playtest_->set_input(playtest_ship_, input);
    const float step = 1.0F / static_cast<float>(tuning::tick_rate);
    playtest_lag_ += frame_seconds_;
    while (playtest_lag_ >= step) {
        playtest_->tick(core::step_from_rate(tuning::tick_rate));
        playtest_lag_ -= step;
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
        session_->state() == ClientState::InGame || session_->state() == ClientState::Binding ||
        session_->state() == ClientState::Lobby || session_->state() == ClientState::InRoom;
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
    case ClientState::Lobby:
        // #304: the rooms, refreshed by the session; a refusal of the server is said once.
        if (screen_ == ClientScreen::Joining) {
            room_list_ = RoomListForm{};
            room_error_.clear();
            screen_ = ClientScreen::Rooms;
        }
        room_list_.update(session_->rooms());
        if (session_->room_error() != room_error_) {
            room_error_ = session_->room_error();
            if (!room_error_.empty()) {
                message_ = {.text = sentence(room_error_), .error = true};
            }
        }
        break;
    case ClientState::InRoom:
        if (screen_ == ClientScreen::Rooms) {
            message_ = {};
            screen_ = ClientScreen::Room;
        }
        break;
    case ClientState::Binding:
        if (screen_ == ClientScreen::Rooms || screen_ == ClientScreen::Room) {
            screen_ = ClientScreen::Joining;
        }
        break;
    case ClientState::Idle:
    case ClientState::Connecting:
    case ClientState::LoggingIn:
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
        case ClientScreen::Editor:
            if (editor_.has_value()) {
                draw_editor(renderer_, font_, art_, *editor_, editor_view_,
                            renderer_.to_logical(window_.mouse().position()),
                            options_.edit.value_or("").filename().string(), preferences_);
                if (!editor_message_.text.empty()) {
                    draw_editor_message(renderer_, font_, editor_message_, preferences_);
                }
            }
            break;
        case ClientScreen::Playtest:
            if (playtest_) {
                draw_playtest(renderer_, font_, art_, *playtest_, playtest_ship_, playtest_wave_,
                              preferences_);
            }
            break;
        case ClientScreen::Rooms:
            draw_rooms(renderer_, font_, room_list_, message_,
                       std::chrono::duration<double>{now - started_}.count(), preferences_);
            break;
        case ClientScreen::Room:
            if (session_.has_value()) {
                const RoomPlayers& room = session_->room_players();
                const auto listed =
                    std::ranges::find(room_list_.rooms(), room.room, &RoomRecord::room);
                draw_room(renderer_, font_,
                          listed != room_list_.rooms().end() ? std::string_view{listed->name}
                                                             : std::string_view{},
                          room, session_->login().value_or(LoginAccepted{}).player, message_,
                          preferences_);
            }
            break;
        case ClientScreen::Connect:
        case ClientScreen::Game:
            draw_connect(renderer_, font_, fields_, message_,
                         ConnectView{.row = row_, .capturing = capture_.waiting().has_value()},
                         preferences_);
            break;
        }
    }
    if (session_.has_value() && (screen_ == ClientScreen::Rooms || screen_ == ClientScreen::Room ||
                                 screen_ == ClientScreen::Game)) {
        session_->chat().prune(now);
        draw_chat(renderer_, font_, session_->chat().shown(), chat_field_, chat_tree_, chatting_,
                  preferences_);
    }
    // Entities only in game; the menus have none.
    const std::optional<std::size_t> entities =
        screen_ == ClientScreen::Game && session_.has_value()
            ? std::optional{session_->world().registry().size()}
            : std::nullopt;
    // The network numbers while connected to a game.
    const std::vector<std::string> network =
        screen_ == ClientScreen::Game && session_.has_value() && !lagometer_.samples().empty()
            ? lag_lines(lagometer_.latest())
            : std::vector<std::string>{};
    metrics_overlay_.draw(renderer_, font_, metrics_, entities, network);
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
