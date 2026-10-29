#include <aegis2d/core/module.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/module.hpp>
#include <aegis2d/net/traffic.hpp>

#include <rtype/bot.hpp>
#include <rtype/bot_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/input_script.hpp>
#include <rtype/login.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using Clock = rtype::Bot::Clock;

constexpr std::string_view version = RTYPE_VERSION;
// How long the network runs between two updates of the bot.
constexpr std::chrono::milliseconds network_slice{5};
// How long the network runs after leaving, for the Logout to leave.
constexpr std::chrono::milliseconds goodbye{200};

// Lock-free, so setting it from a signal handler is safe. Global: a signal handler can only
// reach globals.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> stopping{false};
static_assert(std::atomic<bool>::is_always_lock_free);

extern "C" void on_stop_signal(int /*signal*/) {
    stopping.store(true);
}

// A bot of the process, with when it joined and left.
struct Player {
    std::unique_ptr<rtype::Bot> bot;
    std::string name;
    Clock::time_point joined{};
    Clock::time_point left{};
    bool done = false;
};

// Runs a bot one step: says when it joins, leaves after --seconds, reports why it stopped.
// Returns the exit status it calls for: 1 when it could not join or lost the server.
int step(Player& player, const rtype::BotOptions& options, Clock::time_point now) {
    player.bot->update(now);
    const rtype::ClientSession& session = player.bot->session();
    if (session.state() == rtype::ClientState::Failed ||
        session.state() == rtype::ClientState::Lost) {
        std::cerr << "r-type_bot: " << player.name << ": " << session.error() << '\n';
        player.done = true;
        player.left = now;
        return 1;
    }
    if (session.state() != rtype::ClientState::InGame) {
        return 0;
    }
    if (player.joined == Clock::time_point{}) {
        player.joined = now;
        std::cout << player.name << " joined " << options.settings.server.host << ':'
                  << options.settings.server.tcp_port << " as player "
                  << (session.login().value_or(rtype::LoginAccepted{}).player + 1) << '\n';
    }
    if (options.seconds.has_value() &&
        now - player.joined >= std::chrono::seconds{*options.seconds}) {
        player.bot->leave();
        player.done = true;
        player.left = now;
    }
    return 0;
}

// What a bot that played exchanged with the server.
void report(const Player& player) {
    if (player.joined == Clock::time_point{}) {
        return;
    }
    const rtype::ClientSession& session = player.bot->session();
    const auto received = session.traffic().total(aegis2d::net::Direction::Received,
                                                  aegis2d::net::PacketType::Snapshot);
    std::cout << player.name << " left after "
              << std::chrono::duration<double>{player.left - player.joined}.count()
              << " s: " << session.inputs_sent() << " inputs sent (" << player.bot->picks()
              << " holds), " << received.datagrams << " snapshots received\n";
}

} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::vector<std::string_view> arguments(args.begin() + 1, args.end());
    const std::variant<rtype::BotOptions, std::string> parsed = rtype::parse_bot_options(arguments);
    if (const std::string* const error = std::get_if<std::string>(&parsed)) {
        std::cerr << "r-type_bot: " << *error << '\n' << rtype::bot_usage;
        return 2;
    }
    // Not an error, so the options: get_if rather than get, which could throw.
    rtype::BotOptions options = *std::get_if<rtype::BotOptions>(&parsed);
    if (options.command == rtype::BotCommand::Help) {
        std::cout << rtype::bot_usage;
        return 0;
    }
    if (options.command == rtype::BotCommand::Version) {
        std::cout << "r-type_bot " << version << '\n';
        return 0;
    }
    if (!options.seeded) {
        options.settings.seed = std::random_device{}();
    }
    if (options.inputs.has_value()) {
        std::variant<rtype::InputScript, std::string> script =
            rtype::load_input_script(*options.inputs);
        if (const std::string* const error = std::get_if<std::string>(&script)) {
            std::cerr << "r-type_bot: " << *error << '\n';
            return 1;
        }
        options.settings.script = std::move(*std::get_if<rtype::InputScript>(&script));
    }
    std::cout << "r-type_bot " << version << " (engine modules: " << aegis2d::core::name() << ", "
              << aegis2d::net::name() << "), " << options.count
              << (options.count == 1 ? " bot, " : " bots, ");
    if (options.inputs.has_value()) {
        std::cout << "inputs " << options.inputs->string() << '\n';
    } else {
        std::cout << "seed " << options.settings.seed << '\n';
    }
    // Ctrl+C leaves the server cleanly; without a handler, it ends the bots at once.
    static_cast<void>(std::signal(SIGINT, on_stop_signal));
    static_cast<void>(std::signal(SIGTERM, on_stop_signal));

    try {
        aegis2d::net::NetworkContext context;
        std::vector<Player> players;
        for (const rtype::BotSettings& settings :
             rtype::bot_team(options.settings, options.count)) {
            players.push_back(Player{.bot = std::make_unique<rtype::Bot>(context, settings),
                                     .name = settings.server.name});
        }
        for (Player& player : players) {
            player.bot->start();
        }
        int status = 0;
        const auto playing = [&players] {
            return std::ranges::any_of(players, [](const Player& player) { return !player.done; });
        };
        while (!stopping.load() && playing()) {
            context.run_for(network_slice);
            const Clock::time_point now = Clock::now();
            for (Player& player : players) {
                if (!player.done) {
                    status = std::max(status, step(player, options, now));
                }
            }
        }
        // Ctrl+C: every bot still in game leaves.
        for (Player& player : players) {
            if (!player.done) {
                player.bot->leave();
                player.left = Clock::now();
            }
        }
        const Clock::time_point end = Clock::now() + goodbye;
        while (Clock::now() < end) {
            context.run_for(network_slice);
            for (Player& player : players) {
                player.bot->update(Clock::now());
            }
        }
        for (const Player& player : players) {
            report(player);
        }
        return status;
    } catch (const std::exception& error) {
        std::cerr << "r-type_bot: " << error.what() << '\n';
        return 1;
    }
}
