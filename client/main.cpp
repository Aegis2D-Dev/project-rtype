#include <aegis2d/audio/module.hpp>
#include <aegis2d/core/module.hpp>
#include <aegis2d/ecs/module.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/module.hpp>
#include <aegis2d/render/module.hpp>

#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/login.hpp>

#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
constexpr std::string_view version = RTYPE_VERSION;
// How long the network runs between two updates of the session's timers.
constexpr std::chrono::milliseconds network_slice{20};
// Time left to the Logout to leave when the client quits.
constexpr std::chrono::milliseconds goodbye{200};
} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::vector<std::string_view> arguments(args.begin() + 1, args.end());
    const std::variant<rtype::ClientOptions, std::string> parsed =
        rtype::parse_client_options(arguments);
    if (const std::string* const error = std::get_if<std::string>(&parsed)) {
        std::cerr << "r-type_client: " << *error << '\n' << rtype::client_usage;
        return 2;
    }
    // Not an error, so the options: get_if rather than get, which could throw.
    const auto& options = *std::get_if<rtype::ClientOptions>(&parsed);
    if (options.command == rtype::ClientCommand::Help) {
        std::cout << rtype::client_usage;
        return 0;
    }
    if (options.command == rtype::ClientCommand::Version) {
        std::cout << "r-type_client " << version << '\n';
        return 0;
    }
    std::cout << "r-type_client " << version << " (engine modules: " << aegis2d::core::name()
              << ", " << aegis2d::ecs::name() << ", " << aegis2d::net::name() << ", "
              << aegis2d::render::name() << ", " << aegis2d::audio::name() << ")\n";

    try {
        // Until the window and the game loop exist, the client joins the server, reports it,
        // and leaves.
        aegis2d::net::NetworkContext context;
        rtype::ClientSession session{context, options.settings};
        std::cout << "joining " << options.settings.host << ':' << options.settings.tcp_port
                  << " as " << options.settings.name << '\n';
        session.start();
        while (session.state() != rtype::ClientState::InGame &&
               session.state() != rtype::ClientState::Failed) {
            context.run_for(network_slice);
            session.update(rtype::ClientSession::Clock::now());
        }
        if (session.state() == rtype::ClientState::Failed) {
            std::cerr << "r-type_client: " << session.error() << '\n';
            return 1;
        }
        std::cout << "in game as player "
                  << static_cast<unsigned>(session.login().value_or(rtype::LoginAccepted{}).player)
                  << '\n';
        session.leave();
        const auto deadline = rtype::ClientSession::Clock::now() + goodbye;
        while (rtype::ClientSession::Clock::now() < deadline) {
            context.run_for(network_slice);
            session.update(rtype::ClientSession::Clock::now());
        }
    } catch (const std::exception& error) {
        std::cerr << "r-type_client: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
