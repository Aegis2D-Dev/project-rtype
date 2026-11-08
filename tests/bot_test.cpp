#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/traffic.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <rtype/bot.hpp>
#include <rtype/bot_options.hpp>
#include <rtype/bot_stats.hpp>
#include <rtype/client_session.hpp>
#include <rtype/events.hpp>
#include <rtype/game_server.hpp>
#include <rtype/input_script.hpp>
#include <rtype/login.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using aegis2d::ecs::Action;
using aegis2d::ecs::PlayerInput;
using aegis2d::net::NetworkContext;
using Catch::Matchers::ContainsSubstring;
using rtype::Bot;
using rtype::BotOptions;
using rtype::BotSettings;
using rtype::ClientState;
using rtype::InputScript;
using rtype::RandomInputs;
using rtype::ScriptedInputs;
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
    server.on_input(
        [&](const rtype::Session& /*session*/, PlayerInput input, std::uint16_t /*sequence*/) {
            received.insert(input.held);
            ++inputs;
        });
    Bot bot{context, rtype::BotSettings{.server = {.host = "127.0.0.1",
                                                   .tcp_port = server.tcp_endpoint().port(),
                                                   .name = "B1"},
                                        .seed = 3,
                                        .script = std::nullopt}};
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
    REQUIRE(pump([&] { return received.size() >= 3 && bot.picks() >= 3; }));
    CHECK(inputs > 10);
    CHECK(bot.session().inputs_sent() >= inputs);
    CHECK(server.sessions().size() == 1);

    // What --stats writes: the bot's side of the link.
    const rtype::BotStats stats =
        rtype::measure_bot(bot, "B1", std::chrono::seconds{2}, Clock::now());
    CHECK(stats.name == "B1");
    CHECK(stats.seconds == 2.0);
    CHECK(stats.inputs_sent == bot.session().inputs_sent());
    CHECK(stats.bytes_out_per_second > 0.0);
    CHECK(stats.rtt_ms > 0.0);
    CHECK(stats.loss_percent == 0.0);

    bot.leave();
    CHECK(bot.session().state() == ClientState::Closed);
    REQUIRE(pump([&] { return server.sessions().size() == 0; }));
    CHECK(left == std::optional{rtype::LeaveReason::Left});
    CHECK(server.timeouts() == 0);
    CHECK(server.errors() == 0);
}

namespace {

std::uint8_t held(std::initializer_list<Action> actions) {
    PlayerInput input;
    for (const Action action : actions) {
        input.set(action, true);
    }
    return input.held;
}

InputScript script(std::string_view text) {
    std::variant<InputScript, std::string> parsed = rtype::parse_input_script(text);
    REQUIRE(std::holds_alternative<InputScript>(parsed));
    return std::get<InputScript>(std::move(parsed));
}

std::string script_error(std::string_view text) {
    const std::variant<InputScript, std::string> parsed = rtype::parse_input_script(text);
    REQUIRE(std::holds_alternative<std::string>(parsed));
    return std::get<std::string>(parsed);
}

} // namespace

TEST_CASE("an input script lists actions and how long they are held", "[bot][script]") {
    const InputScript dodge = script("# Dodge while firing.\r\n500 UF\r\n\n  500\tDF  \n250 -\n");
    REQUIRE(dodge.steps.size() == 3);
    CHECK(dodge.steps.at(0).duration == milliseconds{500});
    CHECK(dodge.steps.at(0).held == held({Action::Up, Action::Fire}));
    CHECK(dodge.steps.at(1).held == held({Action::Down, Action::Fire}));
    CHECK(dodge.steps.at(2).duration == milliseconds{250});
    CHECK(dodge.steps.at(2).held == 0);
    CHECK(script("100 LRUDF").steps.at(0).held == 0x1F);

    CHECK_THAT(script_error("500 UF\nfast UF"), ContainsSubstring("line 2"));
    CHECK_THAT(script_error("0 UF"), ContainsSubstring("not a duration"));
    CHECK_THAT(script_error("-5 UF"), ContainsSubstring("not a duration"));
    CHECK_THAT(script_error("60001 UF"), ContainsSubstring("not a duration"));
    CHECK_THAT(script_error("500"), ContainsSubstring("no actions"));
    CHECK_THAT(script_error("# only\n500 UX"), ContainsSubstring("line 2: \"X\""));
    CHECK_THAT(script_error("# nothing\n\n"), ContainsSubstring("no step"));
    const std::variant<InputScript, std::string> missing =
        rtype::load_input_script(std::filesystem::path{"no/such/inputs.txt"});
    REQUIRE(std::holds_alternative<std::string>(missing));
    CHECK_THAT(std::get<std::string>(missing), ContainsSubstring("inputs.txt"));
}

TEST_CASE("a script is played on time and loops", "[bot][script]") {
    ScriptedInputs inputs{script("500 UF\n500 DF\n250 -")};
    const auto at = [&](int ms) { return inputs.update(start + milliseconds{ms}).held; };
    CHECK(at(0) == held({Action::Up, Action::Fire}));
    CHECK(at(499) == held({Action::Up, Action::Fire}));
    CHECK(at(500) == held({Action::Down, Action::Fire}));
    CHECK(at(1000) == 0);
    CHECK(at(1250) == held({Action::Up, Action::Fire}));
    CHECK(inputs.picks() == 4);
    // After a stall, the steps it covered are skipped: 2600 ms is 100 ms into a loop.
    CHECK(at(2600) == held({Action::Up, Action::Fire}));
    CHECK(at(3100) == held({Action::Down, Action::Fire}));
    CHECK(ScriptedInputs{InputScript{}}.update(start).held == 0);
}

TEST_CASE("bots of one process get their own names and seeds", "[bot]") {
    BotSettings base;
    base.seed = 10;
    base.script = script("100 F");
    const std::vector<BotSettings> one = rtype::bot_team(base, 1);
    REQUIRE(one.size() == 1);
    CHECK(one.front().server.name == "bot");
    CHECK(one.front().seed == 10);
    const std::vector<BotSettings> four = rtype::bot_team(base, 4);
    REQUIRE(four.size() == 4);
    for (std::size_t index = 0; index < four.size(); ++index) {
        CHECK(four.at(index).server.name == std::format("bot{}", index + 1));
        CHECK(four.at(index).seed == 10 + index);
        CHECK(four.at(index).script.has_value());
    }

    const auto given = parse({"--count", "4", "--inputs", "dodge.txt"});
    REQUIRE(std::holds_alternative<BotOptions>(given));
    CHECK(std::get<BotOptions>(given).count == 4);
    CHECK(std::get<BotOptions>(given).inputs == std::optional{std::filesystem::path{"dodge.txt"}});
    CHECK(std::get<BotOptions>(parse({})).count == 1);
    CHECK(std::get<BotOptions>(parse({"--count", "64"})).count == 64);
    CHECK_THAT(std::get<std::string>(parse({"--count", "0"})), ContainsSubstring("--count"));
    CHECK_THAT(std::get<std::string>(parse({"--count", "65"})), ContainsSubstring("--count"));
    CHECK_THAT(std::get<std::string>(parse({"--inputs"})), ContainsSubstring("needs a value"));
}

TEST_CASE("four bots fill a game and play their script", "[bot][script]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    // The actions each session held, by token.
    std::map<std::uint32_t, std::set<std::uint8_t>> received;
    server.on_input(
        [&](const rtype::Session& session, PlayerInput input, std::uint16_t /*sequence*/) {
            received[session.token].insert(input.held);
        });
    BotSettings base;
    base.server.tcp_port = server.tcp_endpoint().port();
    base.script = script("200 RF\n200 LF\n100 -");
    std::vector<std::unique_ptr<Bot>> bots;
    for (const BotSettings& settings : rtype::bot_team(base, 5)) {
        bots.push_back(std::make_unique<Bot>(context, settings));
    }
    const auto pump = [&](auto condition) {
        const auto deadline = Clock::now() + std::chrono::seconds{10};
        while (!condition()) {
            if (Clock::now() > deadline) {
                return false;
            }
            context.run_for(milliseconds{5});
            server.update(Clock::now());
            for (const std::unique_ptr<Bot>& bot : bots) {
                bot->update(Clock::now());
            }
        }
        return true;
    };
    const auto in_game = [&] {
        return static_cast<std::size_t>(std::ranges::count_if(
            bots, [](const auto& bot) { return bot->session().state() == ClientState::InGame; }));
    };

    // The first four take the four places; the fifth is told the server is full.
    for (std::size_t index = 0; index < 4; ++index) {
        bots.at(index)->start();
    }
    REQUIRE(pump([&] { return in_game() == 4; }));
    bots.back()->start();
    REQUIRE(pump([&] { return bots.back()->session().state() == ClientState::Failed; }));
    CHECK_THAT(bots.back()->session().error(), ContainsSubstring("full"));
    CHECK(server.sessions().size() == 4);
    std::set<std::uint8_t> players;
    for (std::size_t index = 0; index < 4; ++index) {
        players.insert(bots.at(index)->session().login().value_or(rtype::LoginAccepted{}).player);
    }
    CHECK(players == std::set<std::uint8_t>{0, 1, 2, 3});

    // Each plays the script: the server applies its three steps from all four.
    const std::set<std::uint8_t> steps{held({Action::Right, Action::Fire}),
                                       held({Action::Left, Action::Fire}), 0};
    REQUIRE(pump([&] {
        return received.size() == 4 && std::ranges::all_of(received, [&](const auto& entry) {
                   return entry.second == steps;
               });
    }));
    CHECK(in_game() == 4);
    CHECK(server.timeouts() == 0);

    for (const std::unique_ptr<Bot>& bot : bots) {
        bot->leave();
    }
    REQUIRE(pump([&] { return server.sessions().size() == 0; }));
    CHECK(server.errors() == 0);
}

TEST_CASE("two games of four bots run side by side at 60 ticks per second each", "[bot][room]") {
    NetworkContext context;
    rtype::GameServer game_server{
        context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0, rtype::tuning::tick_rate, 2};
    ServerNetwork& server = game_server.network();
    BotSettings base;
    base.server.tcp_port = server.tcp_endpoint().port();
    std::vector<std::unique_ptr<Bot>> bots;
    for (const BotSettings& settings : rtype::bot_team(base, 8)) {
        bots.push_back(std::make_unique<Bot>(context, settings));
        bots.back()->start();
    }
    const auto pump = [&](auto condition) {
        const auto deadline = Clock::now() + std::chrono::seconds{10};
        while (!condition()) {
            if (Clock::now() > deadline) {
                return false;
            }
            context.run_for(milliseconds{5});
            server.update(Clock::now());
            for (const std::unique_ptr<Bot>& bot : bots) {
                bot->update(Clock::now());
            }
        }
        return true;
    };
    REQUIRE(pump([&] {
        return std::ranges::all_of(
            bots, [](const auto& bot) { return bot->session().state() == ClientState::InGame; });
    }));

    // The first four bots fill the first room, the next four open the second.
    CHECK(game_server.open_rooms() == 2);
    std::array<std::size_t, 2> players{};
    for (const std::unique_ptr<Bot>& bot : bots) {
        const rtype::Session* const session =
            server.sessions().find(bot->session().login().value_or(rtype::LoginAccepted{}).session);
        REQUIRE(session != nullptr);
        ++players.at(session->room);
    }
    CHECK(players == std::array<std::size_t, 2>{4, 4});

    // Both games tick at their rate at the same time, each on its own thread.
    const std::array<std::uint64_t, 2> first{game_server.room(0).simulation().ticks(),
                                             game_server.room(1).simulation().ticks()};
    const auto start = Clock::now();
    REQUIRE(pump([&] { return Clock::now() - start >= std::chrono::seconds{1}; }));
    const std::chrono::duration<double> elapsed = Clock::now() - start;
    for (std::uint32_t room = 0; room < 2; ++room) {
        const double rate =
            static_cast<double>(game_server.room(room).simulation().ticks() - first.at(room)) /
            elapsed.count();
        CAPTURE(room, rate);
        CHECK(rate > 50.0);
    }
    CHECK(server.timeouts() == 0);

    // Everyone leaves: both rooms close, their threads stopped and joined.
    for (const std::unique_ptr<Bot>& bot : bots) {
        bot->leave();
    }
    REQUIRE(pump([&] { return game_server.open_rooms() == 0; }));
    CHECK(server.sessions().size() == 0);
    CHECK(server.errors() == 0);
}

TEST_CASE("the stats of a bot are one line of CSV", "[bot][stats]") {
    const rtype::BotStats stats{.name = "bot2",
                                .seconds = 30.0004,
                                .inputs_sent = 1801,
                                .snapshots = 600,
                                .bytes_in_per_second = 2864.44,
                                .bytes_out_per_second = 1140.0,
                                .rtt_ms = 312.456,
                                .rtt_jitter_ms = 4.5,
                                .loss_percent = 5.0,
                                .snapshot_jitter_ms = 12.345};
    CHECK(rtype::bot_stats_line(stats) ==
          "bot2,30.000,1801,600,2864.4,1140.0,312.46,4.50,5.00,12.35");
    // As many values as columns.
    const std::string header{rtype::bot_stats_header};
    CHECK(std::ranges::count(header, ',') == std::ranges::count(rtype::bot_stats_line(stats), ','));
    CHECK(header.starts_with("bot,seconds,"));

    // A bot that never played measured nothing.
    NetworkContext context;
    const Bot idle{context, BotSettings{}};
    const rtype::BotStats none = rtype::measure_bot(idle, "bot", {}, Clock::now());
    CHECK(none.bytes_in_per_second == 0.0);
    CHECK(none.rtt_ms == 0.0);

    const auto given = parse({"--stats", "bots.csv"});
    REQUIRE(std::holds_alternative<BotOptions>(given));
    CHECK(std::get<BotOptions>(given).stats == std::optional{std::filesystem::path{"bots.csv"}});
    CHECK(!std::get<BotOptions>(parse({})).stats.has_value());
}
