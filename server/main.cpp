#include <aegis2d/core/config.hpp>
#include <aegis2d/core/log.hpp>
#include <aegis2d/core/module.hpp>
#include <aegis2d/ecs/module.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/module.hpp>
#include <aegis2d/net/socket_error.hpp>

#include <rtype/game_instance.hpp>
#include <rtype/game_server.hpp>
#include <rtype/server_game.hpp>
#include <rtype/server_network.hpp>
#include <rtype/server_options.hpp>
#include <rtype/waves.hpp>

#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
constexpr std::string_view version = RTYPE_VERSION;
// How long the network runs between two checks of the login timeouts.
constexpr std::chrono::milliseconds network_slice{50};
} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::vector<std::string_view> arguments(args.begin() + 1, args.end());
    const std::variant<rtype::ServerOptions, std::string> parsed =
        rtype::parse_server_options(arguments);
    if (const std::string* const error = std::get_if<std::string>(&parsed)) {
        std::cerr << "r-type_server: " << *error << '\n' << rtype::server_usage;
        return 2;
    }
    // Not an error, so the options: get_if rather than get, which could throw.
    const auto& options = *std::get_if<rtype::ServerOptions>(&parsed);
    if (options.command == rtype::ServerCommand::Help) {
        std::cout << rtype::server_usage;
        return 0;
    }
    if (options.command == rtype::ServerCommand::Version) {
        std::cout << "r-type_server " << version << '\n';
        return 0;
    }
    std::cout << "r-type_server " << version << " (engine modules: " << aegis2d::core::name()
              << ", " << aegis2d::ecs::name() << ", " << aegis2d::net::name() << ")\n";

    const std::filesystem::path waves_file = options.waves.value_or(rtype::default_waves_file());
    std::optional<rtype::GameInstance> instance;
    try {
        instance.emplace(std::nullopt, rtype::load_waves(waves_file));
    } catch (const aegis2d::core::ConfigError& error) {
        std::cerr << "r-type_server: " << error.what() << '\n';
        return 1;
    }
    const rtype::ServerGame& game = instance->game();
    const aegis2d::ecs::WaveSchedule& waves = game.waves();
    std::cout << "waves: " << waves_file.string() << " (" << waves.waves()
              << (waves.waves() == 1 ? " wave, " : " waves, ") << waves.spawns().size()
              << " enemies in " << waves.duration() << " s" << (waves.loops() ? ", looping" : "")
              << ")\n";
    std::cout << "systems:";
    for (const std::string_view system : game.systems().names()) {
        std::cout << ' ' << system;
    }
    std::cout << '\n';

    aegis2d::net::NetworkContext context;
    try {
        rtype::GameServer server{context, *instance, options.tcp_port, options.udp_port};
        rtype::ServerNetwork& network = server.network();
        // Flushed, so scripts waiting for the ports see it while the server runs.
        std::cout << "listening: TCP port " << network.tcp_endpoint().port() << ", UDP port "
                  << network.udp_endpoint().port() << ", simulation at "
                  << server.simulation().rate() << " Hz\n"
                  << std::flush;
        // Runs until the process is stopped (Ctrl+C); the clean shutdown is #121. The network
        // handlers run in run_for(), the login timeouts in update() between two calls.
        while (true) {
            // The handling of each client is contained by ServerNetwork; anything else that
            // throws (a task posted by the game) is logged, and the server goes on.
            try {
                context.run_for(network_slice);
                network.update(rtype::ServerNetwork::Clock::now());
            } catch (const std::exception& error) {
                aegis2d::core::log_error("server: {}", error.what());
            }
        }
    } catch (const aegis2d::net::SocketError& error) {
        std::cerr << "r-type_server: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
