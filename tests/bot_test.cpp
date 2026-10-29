#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/traffic.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <rtype/bot.hpp>
#include <rtype/bot_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/events.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using aegis2d::ecs::Action;
using aegis2d::ecs::PlayerInput;
using aegis2d::net::NetworkContext;
using Catch::Matchers::ContainsSubstring;
using rtype::Bot;
using rtype::BotOptions;
using rtype::ClientState;
using rtype::RandomInputs;
using rtype::ServerNetwork;
using std::chrono::milliseconds;
using Clock = RandomInputs::Clock;

namespace {

constexpr Clock::time_point start = Clock::time_point{} + std::chrono::hours{1};

// The inputs picked over @p seconds, one frame every 10 ms, with the time each was picked.
struct Hold {
    std::uint8_t held = 0;
    milliseconds time{};
};

std::vector<Hold> holds(std::uint32_t seed, int seconds) {
    RandomInputs inputs{seed};
    std::vector<Hold> result;
    for (milliseconds at{}; at < std::chrono::seconds{seconds}; at += milliseconds{10}) {
        const PlayerInput input = inputs.update(start + at);
        if (result.size() < inputs.picks()) {
            result.push_back(Hold{.held = input.held, .time = at});
        }
    }
    return result;
}

std::variant<BotOptions, std::string> parse(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> list{args};
    return rtype::parse_bot_options(list);
}

} // namespace

TEST_CASE("random inputs are held for a random time and repeat with their seed", "[bot]") {
    const std::vector<Hold> first = holds(7, 300);
    // 5 minutes of holds of 150 to 900 ms: between 333 and 2000 picks.
    REQUIRE(first.size() > 333);
    REQUIRE(first.size() < 2000);
    std::set<std::uint8_t> seen;
    for (std::size_t index = 0; index < first.size(); ++index) {
        // Only the five actions; never left and right, nor up and down, at once.
        const std::uint8_t held = first.at(index).held;
        CHECK((held & 0xE0U) == 0U);
        const PlayerInput input{.held = held};
        CHECK(!(input.holds(Action::Left) && input.holds(Action::Right)));
        CHECK(!(input.holds(Action::Up) && input.holds(Action::Down)));
        seen.insert(held);
        if (index > 0) {
            const milliseconds held_for = first.at(index).time - first.at(index - 1).time;
            CHECK(held_for >= RandomInputs::shortest_hold);
            CHECK(held_for <= RandomInputs::longest_hold + milliseconds{10});
        }
    }
    // 3 x 3 directions, with and without Fire.
    CHECK(seen.size() == 18);

    const std::vector<Hold> again = holds(7, 300);
    REQUIRE(again.size() == first.size());
    for (std::size_t index = 0; index < first.size(); ++index) {
        CHECK(again.at(index).held == first.at(index).held);
        CHECK(again.at(index).time == first.at(index).time);
    }
    const std::vector<Hold> other = holds(8, 300);
    bool differs = other.size() != first.size();
    for (std::size_t index = 0; !differs && index < first.size(); ++index) {
        differs = other.at(index).held != first.at(index).held;
    }
    CHECK(differs);
}

TEST_CASE("the bot's command line has defaults and checks its values", "[bot]") {
    const auto defaults = parse({});
    REQUIRE(std::holds_alternative<BotOptions>(defaults));
    const auto& options = std::get<BotOptions>(defaults);
    CHECK(options.command == rtype::BotCommand::Run);
    CHECK(options.settings.server.host == "127.0.0.1");
    CHECK(options.settings.server.tcp_port == 4242);
    CHECK(options.settings.server.name == "bot");
    CHECK(!options.seeded);
    CHECK(!options.seconds.has_value());

    const auto given = parse({"--host", "10.0.0.2", "--port", "5000", "--name", "B1", "--seconds",
                              "90", "--seed", "4294967295"});
    REQUIRE(std::holds_alternative<BotOptions>(given));
    const auto& set = std::get<BotOptions>(given);
    CHECK(set.settings.server.host == "10.0.0.2");
    CHECK(set.settings.server.tcp_port == 5000);
    CHECK(set.settings.server.name == "B1");
    CHECK(set.seconds == std::optional<std::uint32_t>{90});
    CHECK(set.seeded);
    CHECK(set.settings.seed == 4294967295U);

    CHECK(std::get<BotOptions>(parse({"--name", "B1", "--help"})).command ==
          rtype::BotCommand::Help);
    CHECK(std::get<BotOptions>(parse({"--version"})).command == rtype::BotCommand::Version);
    CHECK_THAT(std::get<std::string>(parse({"--fast"})), ContainsSubstring("unknown argument"));
    CHECK_THAT(std::get<std::string>(parse({"--port"})), ContainsSubstring("needs a value"));
    CHECK_THAT(std::get<std::string>(parse({"--port", "0"})), ContainsSubstring("--port"));
    CHECK_THAT(std::get<std::string>(parse({"--seconds", "0"})), ContainsSubstring("--seconds"));
    CHECK_THAT(std::get<std::string>(parse({"--seconds", "-5"})), ContainsSubstring("--seconds"));
    CHECK_THAT(std::get<std::string>(parse({"--seed", "4294967296"})), ContainsSubstring("--seed"));
}

TEST_CASE("a bot joins, plays and leaves cleanly", "[bot]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<std::string> joined;
    std::optional<rtype::LeaveReason> left;
    server.on_players(
        [&](const rtype::Session& session) { joined.push_back(session.name); },
        [&](const rtype::Session& /*session*/, rtype::LeaveReason reason) { left = reason; });
    std::set<std::uint8_t> received;
    std::size_t inputs = 0;
    server.on_input([&](const rtype::Session& /*session*/, PlayerInput input) {
        received.insert(input.held);
        ++inputs;
    });
    Bot bot{context, rtype::BotSettings{.server = {.host = "127.0.0.1",
                                                   .tcp_port = server.tcp_endpoint().port(),
                                                   .name = "B1"},
                                        .seed = 3}};
    const auto pump = [&](auto condition) {
        const auto deadline = Clock::now() + std::chrono::seconds{10};
        while (!condition()) {
            if (Clock::now() > deadline) {
                return false;
            }
            context.run_for(milliseconds{5});
            server.update(Clock::now());
            bot.update(Clock::now());
        }
        return true;
    };

    bot.start();
    REQUIRE(pump([&] { return bot.session().state() == ClientState::InGame; }));
    CHECK(joined == std::vector<std::string>{"B1"});
    // The server applies the bot's random inputs: several different ones within seconds.
    REQUIRE(pump([&] { return received.size() >= 3 && bot.inputs().picks() >= 3; }));
    CHECK(inputs > 10);
    CHECK(bot.session().inputs_sent() >= inputs);
    CHECK(server.sessions().size() == 1);

    bot.leave();
    CHECK(bot.session().state() == ClientState::Closed);
    REQUIRE(pump([&] { return server.sessions().size() == 0; }));
    CHECK(left == std::optional{rtype::LeaveReason::Left});
    CHECK(server.timeouts() == 0);
    CHECK(server.errors() == 0);
}
