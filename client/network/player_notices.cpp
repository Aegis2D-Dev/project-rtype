#include <rtype/client_world.hpp>
#include <rtype/events.hpp>
#include <rtype/player_notices.hpp>
#include <rtype/tuning.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

std::string left_message(const std::string& name, LeaveReason reason) {
    switch (reason) {
    case LeaveReason::TimedOut:
        return std::format("{} lost the connection", name);
    case LeaveReason::Kicked:
        return std::format("{} was removed from the game", name);
    case LeaveReason::Left:
        break;
    }
    return std::format("{} left the game", name);
}

void PlayerNotices::handle(const GameEvent& event, Clock::time_point now) {
    const std::uint32_t tick = std::visit([](const auto& any) { return any.tick; }, event);
    if (!first_tick_.has_value()) {
        first_tick_ = tick;
    }
    if (const auto* const joined = std::get_if<PlayerJoinedEvent>(&event)) {
        if (joined->player >= tuning::max_players) {
            return;
        }
        names_.at(joined->player) = joined->name;
        if (tick > *first_tick_) {
            add(joined->player, std::format("{} joined the game", joined->name), now);
        }
    } else if (const auto* const left = std::get_if<PlayerLeftEvent>(&event)) {
        if (left->player >= tuning::max_players) {
            return;
        }
        add(left->player, left_message(name_of(left->player), left->reason), now);
        names_.at(left->player).clear();
    }
}

void PlayerNotices::remember(
    const std::array<std::optional<RemotePlayer>, tuning::max_players>& players) {
    for (std::size_t number = 0; number < players.size(); ++number) {
        const std::optional<RemotePlayer>& player = players.at(number);
        if (player.has_value() && !player->name.empty()) {
            names_.at(number) = player->name;
        }
    }
}

void PlayerNotices::prune(Clock::time_point now) {
    std::erase_if(shown_, [now](const PlayerNotice& notice) { return notice.until <= now; });
}

const std::vector<PlayerNotice>& PlayerNotices::shown() const noexcept {
    return shown_;
}

void PlayerNotices::clear() noexcept {
    names_ = {};
    shown_.clear();
    first_tick_.reset();
}

void PlayerNotices::add(std::uint8_t player, std::string text, Clock::time_point now) {
    if (shown_.size() == max_shown) {
        shown_.erase(shown_.begin());
    }
    shown_.push_back(
        PlayerNotice{.player = player, .text = std::move(text), .until = now + shown_for});
}

std::string PlayerNotices::name_of(std::uint8_t player) const {
    const std::string& name = names_.at(player);
    return name.empty() ? std::format("Player {}", player + 1) : name;
}

} // namespace rtype
