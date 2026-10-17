#include <catch2/catch_test_macros.hpp>
#include <rtype/client_world.hpp>
#include <rtype/events.hpp>
#include <rtype/player_notices.hpp>
#include <rtype/tuning.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

using rtype::GameEvent;
using rtype::LeaveReason;
using rtype::PlayerJoinedEvent;
using rtype::PlayerLeftEvent;
using rtype::PlayerNotices;
using rtype::RemotePlayer;
using namespace std::chrono_literals;

TEST_CASE("the players who stay are told who left the game", "[client][notices]") {
    PlayerNotices notices;
    const PlayerNotices::Clock::time_point now{};
    // When this client joined, the server named the players already there: not announced.
    notices.handle(GameEvent{PlayerJoinedEvent{.tick = 100, .player = 0, .name = "Lolo"}}, now);
    notices.handle(GameEvent{PlayerJoinedEvent{.tick = 100, .player = 2, .name = "Matteo"}}, now);
    CHECK(notices.shown().empty());

    // Someone joins later: announced.
    notices.handle(GameEvent{PlayerJoinedEvent{.tick = 400, .player = 3, .name = "Nept"}}, now);
    REQUIRE(notices.shown().size() == 1);
    CHECK(notices.shown().back().text == "Nept joined the game");
    CHECK(notices.shown().back().player == 3);

    // The world forgets a player as it applies its PlayerLeft; the notices still know it.
    std::array<std::optional<RemotePlayer>, rtype::tuning::max_players> players{};
    players.at(2) = RemotePlayer{.name = "Matteo", .ship = 7, .lives = 3, .score = 0};
    notices.remember(players);
    notices.handle(
        GameEvent{PlayerLeftEvent{.tick = 500, .player = 2, .reason = LeaveReason::Left}},
        now + 1s);
    notices.handle(
        GameEvent{PlayerLeftEvent{.tick = 510, .player = 0, .reason = LeaveReason::TimedOut}},
        now + 2s);
    REQUIRE(notices.shown().size() == 3);
    CHECK(notices.shown().at(1).text == "Matteo left the game");
    CHECK(notices.shown().at(2).text == "Lolo lost the connection");

    // Each message stays five seconds.
    notices.prune(now + 5s);
    CHECK(notices.shown().size() == 2);
    notices.prune(now + 7s);
    CHECK(notices.shown().empty());
}

TEST_CASE("a player whose name never came is called by its number", "[client][notices]") {
    PlayerNotices notices;
    const PlayerNotices::Clock::time_point now{};
    notices.handle(
        GameEvent{PlayerLeftEvent{.tick = 5, .player = 1, .reason = LeaveReason::Kicked}}, now);
    REQUIRE(notices.shown().size() == 1);
    CHECK(notices.shown().front().text == "Player 2 was removed from the game");
    for (std::uint8_t player = 0; player < 4; ++player) {
        notices.handle(GameEvent{PlayerLeftEvent{.tick = 6, .player = player}}, now);
    }
    CHECK(notices.shown().size() == PlayerNotices::max_shown);
    notices.clear();
    CHECK(notices.shown().empty());
}
