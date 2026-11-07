#include <catch2/catch_test_macros.hpp>
#include <rtype/chat.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

using rtype::ChatMessage;
using rtype::ChatRejection;

namespace {

std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for (const unsigned value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

} // namespace

TEST_CASE("chat messages follow the RFC example", "[protocol][chat]") {
    // RFC section 3.8, without the length prefixes.
    const std::vector<std::byte> send = bytes({0x11, 0x02, 0x00, 0x68, 0x69});
    CHECK(rtype::write_chat_send("hi") == send);
    CHECK(rtype::read_chat_send(send) == std::string{"hi"});

    const std::vector<std::byte> relayed =
        bytes({0x12, 0x00, 0x02, 0x00, 0x52, 0x39, 0x02, 0x00, 0x68, 0x69});
    const ChatMessage message{.player = 0, .name = "R9", .text = "hi"};
    CHECK(rtype::write_chat_message(message) == relayed);
    CHECK(rtype::read_chat_message(relayed) == message);

    const std::vector<std::byte> rejected = bytes({0x13, 0x03});
    CHECK(rtype::write_chat_rejected(ChatRejection::TooFast) == rejected);
    CHECK(rtype::read_chat_rejected(rejected) == std::uint8_t{3});
    // An unknown reason is read as sent.
    CHECK(rtype::read_chat_rejected(bytes({0x13, 0x09})) == std::uint8_t{9});
}

TEST_CASE("malformed chat messages are refused", "[protocol][chat]") {
    CHECK(!rtype::read_chat_send(bytes({0x11})).has_value());
    CHECK(!rtype::read_chat_send(bytes({0x11, 0x02, 0x00, 0x68})).has_value());
    CHECK(!rtype::read_chat_send(bytes({0x11, 0x01, 0x00, 0x68, 0x00})).has_value());
    CHECK(!rtype::read_chat_send(bytes({0x12, 0x01, 0x00, 0x68})).has_value());
    CHECK(!rtype::read_chat_message(bytes({0x12, 0x00, 0x00, 0x00})).has_value());
    CHECK(!rtype::read_chat_rejected(bytes({0x13})).has_value());
    CHECK(!rtype::read_chat_rejected(bytes({0x13, 0x01, 0x01})).has_value());
}

TEST_CASE("a chat text is 1 to 200 bytes of UTF-8 without control characters", "[protocol][chat]") {
    CHECK(!rtype::check_chat_text("hi").has_value());
    CHECK(!rtype::check_chat_text(std::string(rtype::max_chat_size, 'a')).has_value());
    CHECK(!rtype::check_chat_text("caf\xC3\xA9 !").has_value());
    // The length first: a long text with a control character is too long.
    CHECK(rtype::check_chat_text(std::string(rtype::max_chat_size + 1, 'a')) ==
          ChatRejection::TooLong);
    CHECK(rtype::check_chat_text(std::string(rtype::max_chat_size + 1, '\n')) ==
          ChatRejection::TooLong);
    CHECK(rtype::check_chat_text("") == ChatRejection::BadText);
    CHECK(rtype::check_chat_text("a\nb") == ChatRejection::BadText);
    CHECK(rtype::check_chat_text("\x7F") == ChatRejection::BadText);
    CHECK(rtype::check_chat_text("\xC3") == ChatRejection::BadText);
    CHECK(rtype::check_chat_text("\xC0\xAF") == ChatRejection::BadText);
    CHECK(rtype::to_string(ChatRejection::TooFast) == "too fast");
}
