#include <aegis2d/core/module.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/module.hpp>
#include <aegis2d/net/traffic.hpp>

#include <rtype/bot.hpp>
#include <rtype/bot_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/login.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <string_view>
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
    std::cout << "r-type_bot " << version << " (engine modules: " << aegis2d::core::name() << ", "
              << aegis2d::net::name() << "), seed " << options.settings.seed << '\n';
    // Ctrl+C leaves the server cleanly; without a handler, it ends the bot at once.
    static_cast<void>(std::signal(SIGINT, on_stop_signal));
    static_cast<void>(std::signal(SIGTERM, on_stop_signal));

    try {
        aegis2d::net::NetworkContext context;
        rtype::Bot bot{context, options.settings};
        bot.start();
        Clock::time_point joined{};
        while (!stopping.load()) {
            context.run_for(network_slice);
            const Clock::time_point now = Clock::now();
            bot.update(now);
            const rtype::ClientState state = bot.session().state();
            if (state == rtype::ClientState::Failed || state == rtype::ClientState::Lost) {
                std::cerr << "r-type_bot: " << bot.session().error() << '\n';
                return 1;
            }
            if (state != rtype::ClientState::InGame) {
                continue;
            }
            if (joined == Clock::time_point{}) {
                joined = now;
                std::cout << "joined " << options.settings.server.host << ':'
                          << options.settings.server.tcp_port << " as player "
                          << (bot.session().login().value_or(rtype::LoginAccepted{}).player + 1)
                          << '\n';
            }
            if (options.seconds.has_value() &&
                now - joined >= std::chrono::seconds{*options.seconds}) {
                break;
            }
        }
        const bool played = joined != Clock::time_point{};
        bot.leave();
        const Clock::time_point end = Clock::now() + goodbye;
        while (Clock::now() < end) {
            context.run_for(network_slice);
            bot.update(Clock::now());
        }
        if (played) {
            const auto received = bot.session().traffic().total(aegis2d::net::Direction::Received,
                                                                aegis2d::net::PacketType::Snapshot);
            std::cout << "left after "
                      << std::chrono::duration<double>{Clock::now() - joined}.count()
                      << " s: " << bot.session().inputs_sent() << " inputs sent ("
                      << bot.inputs().picks() << " different holds), " << received.datagrams
                      << " snapshots received\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "r-type_bot: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
