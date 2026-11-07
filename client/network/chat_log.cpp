#include <rtype/chat.hpp>
#include <rtype/chat_log.hpp>
#include <rtype/login.hpp>
#include <rtype/player_notices.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rtype {

bool ChatLog::add(const ChatMessage& message, Clock::time_point now) {
    if (!valid_player_name(message.name) || check_chat_text(message.text).has_value()) {
        return false;
    }
    push(PlayerNotice{.player = message.player,
                      .text = message.name + ": " + message.text,
                      .until = now + shown_for});
    return true;
}

void ChatLog::refused(std::uint8_t reason, Clock::time_point now) {
    push(PlayerNotice{
        .player = lobby_player, .text = chat_rejection_text(reason), .until = now + shown_for});
}

void ChatLog::prune(Clock::time_point now) {
    std::erase_if(shown_, [now](const PlayerNotice& line) { return line.until <= now; });
}

const std::vector<PlayerNotice>& ChatLog::shown() const noexcept {
    return shown_;
}

void ChatLog::clear() noexcept {
    shown_.clear();
}

void ChatLog::push(PlayerNotice line) {
    if (shown_.size() == max_shown) {
        shown_.erase(shown_.begin());
    }
    shown_.push_back(std::move(line));
}

std::string chat_rejection_text(std::uint8_t reason) {
    switch (static_cast<ChatRejection>(reason)) {
    case ChatRejection::TooLong:
        return "Message not sent: too long";
    case ChatRejection::BadText:
        return "Message not sent: invalid characters";
    case ChatRejection::TooFast:
        return "Message not sent: too fast, wait a moment";
    }
    return "Message not sent";
}

} // namespace rtype
