#include <catch2/catch_test_macros.hpp>
#include <rtype/chat.hpp>
#include <rtype/chat_log.hpp>

#include <chrono>
#include <cstdint>
#include <string>

using rtype::ChatLog;
using rtype::ChatMessage;

TEST_CASE("the chat shows its last messages for a few seconds", "[client][chat]") {
    ChatLog log;
    const ChatLog::Clock::time_point start{};
    CHECK(log.add(ChatMessage{.player = 2, .name = "R9", .text = "hi"}, start));
    REQUIRE(log.shown().size() == 1);
    CHECK(log.shown().front().player == 2);
    CHECK(log.shown().front().text == "R9: hi");

    // A message that breaks the rules of the protocol is not shown.
    CHECK(!log.add(ChatMessage{.player = 0, .name = "", .text = "hi"}, start));
    CHECK(!log.add(ChatMessage{.player = 0, .name = "R9", .text = "a\nb"}, start));
    CHECK(log.shown().size() == 1);

    // A new message pushes out the oldest beyond max_shown.
    for (std::size_t message = 0; message < ChatLog::max_shown; ++message) {
        CHECK(log.add(ChatMessage{.player = 1, .name = "Kaz", .text = std::to_string(message)},
                      start + std::chrono::seconds{1}));
    }
    CHECK(log.shown().size() == ChatLog::max_shown);
    CHECK(log.shown().front().text == "Kaz: 0");

    // What the server refused is told in the lobby's color.
    log.refused(static_cast<std::uint8_t>(rtype::ChatRejection::TooFast),
                start + std::chrono::seconds{2});
    CHECK(log.shown().back().player == rtype::lobby_player);
    CHECK(log.shown().back().text == "Message not sent: too fast, wait a moment");
    CHECK(rtype::chat_rejection_text(42) == "Message not sent");

    log.prune(start + std::chrono::seconds{1} + ChatLog::shown_for);
    REQUIRE(log.shown().size() == 1);
    CHECK(log.shown().front().player == rtype::lobby_player);
    log.clear();
    CHECK(log.shown().empty());
}
