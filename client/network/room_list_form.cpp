#include <rtype/room_list_form.hpp>
#include <rtype/rooms.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace rtype {

void RoomListForm::update(std::span<const RoomRecord> rooms) {
    std::optional<std::uint8_t> selected;
    if (row_ < rooms_.size()) {
        selected = rooms_.at(row_).room;
    }
    const bool creating = row_ == rooms_.size();
    rooms_.assign(rooms.begin(), rooms.end());
    if (creating) {
        row_ = rooms_.size();
        return;
    }
    const auto found =
        std::ranges::find_if(rooms_, [&](const RoomRecord& room) { return selected == room.room; });
    row_ = found != rooms_.end() ? static_cast<std::size_t>(found - rooms_.begin())
                                 : std::min(row_, rooms_.size());
}

const std::vector<RoomRecord>& RoomListForm::rooms() const noexcept {
    return rooms_;
}

std::size_t RoomListForm::row() const noexcept {
    return row_;
}

bool RoomListForm::creating() const noexcept {
    return row_ == rooms_.size();
}

void RoomListForm::up() noexcept {
    row_ = row_ == 0 ? rooms_.size() : row_ - 1;
}

void RoomListForm::down() noexcept {
    row_ = row_ == rooms_.size() ? 0 : row_ + 1;
}

bool RoomListForm::type(char character) {
    if (!creating() || name_.size() >= max_name || character < ' ' || character > '~') {
        return false;
    }
    name_.push_back(character);
    return true;
}

void RoomListForm::erase() noexcept {
    if (creating() && !name_.empty()) {
        name_.pop_back();
    }
}

void RoomListForm::bigger() noexcept {
    size_ = static_cast<std::uint8_t>(std::min<std::size_t>(size_ + 1U, tuning::max_players));
}

void RoomListForm::smaller() noexcept {
    size_ = static_cast<std::uint8_t>(std::max(size_ - 1, 1));
}

const std::string& RoomListForm::name() const noexcept {
    return name_;
}

std::uint8_t RoomListForm::size() const noexcept {
    return size_;
}

std::variant<std::uint8_t, CreateRoomRequest, std::string> RoomListForm::submit() const {
    if (creating()) {
        if (name_.empty()) {
            return std::string{"Type a name for the new room."};
        }
        return CreateRoomRequest{.name = name_, .max_players = size_};
    }
    const RoomRecord& room = rooms_.at(row_);
    if (room.players >= room.max_players) {
        return std::string{"This room is full."};
    }
    return room.room;
}

} // namespace rtype
