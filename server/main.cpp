#include <aegis2d/core/config.hpp>
#include <aegis2d/core/log.hpp>
#include <aegis2d/core/module.hpp>
#include <aegis2d/ecs/module.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/module.hpp>
#include <aegis2d/net/socket_error.hpp>

#include <rtype/game_server.hpp>
#include <rtype/rate_limit.hpp>
#include <rtype/server_game.hpp>
#include <rtype/server_network.hpp>
#include <rtype/server_options.hpp>
#include <rtype/stop_signal.hpp>
#include <rtype/tuning.hpp>
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
// Longest wait for the Logout of each player to leave when the server stops.
constexpr std::chrono::seconds shutdown_grace{1};
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
    std::optional<aegis2d::ecs::WaveSchedule> loaded;
    try {
        loaded = rtype::load_waves(waves_file);
    } catch (const aegis2d::core::ConfigError& error) {
        std::cerr << "r-type_server: " << error.what() << '\n';
        return 1;
    }
    const aegis2d::ecs::WaveSchedule& waves = *loaded;
    // The systems of every game, as a game the room has not started yet runs them.
    const rtype::ServerGame game;
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
        rtype::GameServer server{context,          std::nullopt,     waves,
                                 options.tcp_port, options.udp_port, rtype::tuning::tick_rate,
                                 options.max_rooms};
        rtype::ServerNetwork& network = server.network();
        network.set_limits(rtype::RateLimits{.datagrams_per_second = options.packet_rate});
        network.set_compression(options.compress_from);
        // Flushed, so scripts waiting for the ports see it while the server runs.
        std::cout << "listening: TCP port " << network.tcp_endpoint().port() << ", UDP port "
                  << network.udp_endpoint().port() << ", up to " << server.max_rooms()
                  << (server.max_rooms() == 1 ? " room" : " rooms") << " of "
                  << rtype::tuning::max_players << " players, each simulated at " << server.rate()
                  << " Hz\n"
                  << std::flush;
        if (!rtype::install_stop_handlers()) {
            aegis2d::core::log_warn(
                "server: Ctrl+C will stop the server without telling the players");
        }
        // The network handlers run in run_for(), the timeouts in update() between two calls.
        // The handling of each client is contained by ServerNetwork; anything else that throws
        // (a task posted by the game) is logged, and the server goes on.
        const auto run_network = [&context, &network] {
            try {
                context.run_for(network_slice);
                network.update(rtype::ServerNetwork::Clock::now());
            } catch (const std::exception& error) {
                aegis2d::core::log_error("server: {}", error.what());
            }
        };
        while (!rtype::stop_requested()) {
            run_network();
        }

        // Ctrl+C or SIGTERM: every player gets Logout(Shutdown), then the connections close
        // once it has left; a stuck peer does not hold the stop longer than shutdown_grace.
        aegis2d::core::log_info("server: stopping (Ctrl+C again to quit at once)");
        network.shutdown();
        const auto deadline = rtype::ServerNetwork::Clock::now() + shutdown_grace;
        while (!network.idle() && rtype::ServerNetwork::Clock::now() < deadline) {
            run_network();
        }
        // Leaving the scope stops the simulation threads, then closes the sockets.
    } catch (const aegis2d::net::SocketError& error) {
        std::cerr << "r-type_server: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "r-type_server: " << error.what() << '\n';
        return 1;
    }
    std::cout << "r-type_server: stopped\n";
    return 0;
}
