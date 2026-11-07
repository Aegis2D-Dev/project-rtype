#include <aegis2d/net/binary.hpp>

#include <rtype/chat.hpp>
#include <rtype/login.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

std::string_view to_string(ChatRejection reason) noexcept {
    switch (reason) {
    case ChatRejection::TooLong:
        return "too long";
    case ChatRejection::BadText:
        return "bad text";
    case ChatRejection::TooFast:
        return "too fast";
    }
    return "unknown";
}

std::optional<ChatRejection> check_chat_text(std::string_view text) noexcept {
    if (text.size() > max_chat_size) {
        return ChatRejection::TooLong;
    }
    if (!valid_text(text, max_chat_size)) {
        return ChatRejection::BadText;
    }
    return std::nullopt;
}

std::optional<std::string> read_chat_send(std::span<const std::byte> message) {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::ChatSend)) {
        return std::nullopt;
    }
    std::optional<std::string> text = reader.read_string();
    if (!text || !reader.at_end()) {
        return std::nullopt;
    }
    return text;
}

std::optional<ChatMessage> read_chat_message(std::span<const std::byte> message) {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::ChatMessage)) {
        return std::nullopt;
    }
    const std::optional<std::uint8_t> player = reader.read_u8();
    std::optional<std::string> name = reader.read_string();
    std::optional<std::string> text = reader.read_string();
    if (!player || !name || !text || !reader.at_end()) {
        return std::nullopt;
    }
    return ChatMessage{.player = *player, .name = std::move(*name), .text = std::move(*text)};
}

std::optional<std::uint8_t> read_chat_rejected(std::span<const std::byte> message) noexcept {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<std::uint8_t> reason = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::ChatRejected) || !reason || !reader.at_end()) {
        return std::nullopt;
    }
    return reason;
}

std::vector<std::byte> write_chat_send(std::string_view text) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::ChatSend));
    writer.write_string(text);
    return {writer.data().begin(), writer.data().end()};
}

std::vector<std::byte> write_chat_message(const ChatMessage& message) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::ChatMessage));
    writer.write_u8(message.player);
    writer.write_string(message.name);
    writer.write_string(message.text);
    return {writer.data().begin(), writer.data().end()};
}

std::vector<std::byte> write_chat_rejected(ChatRejection reason) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::ChatRejected));
    writer.write_u8(static_cast<std::uint8_t>(reason));
    return {writer.data().begin(), writer.data().end()};
}

} // namespace rtype
