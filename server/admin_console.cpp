#include <aegis2d/core/commands.hpp>

#include <rtype/admin_console.hpp>
#include <rtype/chat.hpp>
#include <rtype/rooms.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <istream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

namespace core = aegis2d::core;

namespace {

std::string room_line(const RoomRecord& room) {
    return std::format(
        "room {} \"{}\": {}/{} players, {}", room.room, room.name, room.players, room.max_players,
        room.state == static_cast<std::uint8_t>(RoomState::Playing) ? "playing" : "waiting");
}

std::string player_line(const Session& session) {
    const std::string where = session.room == no_room
                                  ? std::string{"lobby"}
                                  : std::format("room {}, player {}", session.room, session.player);
    return std::format("{}: {}, {}", session.name, where,
                       session.endpoint ? "in game from " + session.endpoint->to_string()
                                        : std::string{"out of game"});
}

} // namespace

AdminConsole::AdminConsole(ServerNetwork& network, std::function<void()> stop)
    : network_{&network}, stop_{std::move(stop)} {
    commands_.add("rooms", {}, "Lists the rooms in use", [this](const core::CommandCall&) {
        std::string text;
        for (const RoomRecord& room : network_->sessions().list_rooms()) {
            text += (text.empty() ? "" : "\n") + room_line(room);
        }
        return text.empty() ? std::string{"no room in use"} : text;
    });
    commands_.add("players", {}, "Lists the players, with their room and address",
                  [this](const core::CommandCall&) {
                      const SessionTable& sessions = network_->sessions();
                      std::vector<std::uint32_t> rooms;
                      for (const RoomRecord& room : sessions.list_rooms()) {
                          rooms.push_back(room.room);
                      }
                      rooms.push_back(no_room);
                      std::string text;
                      for (const std::uint32_t room : rooms) {
                          for (const Session& session : sessions.members(room)) {
                              text += (text.empty() ? "" : "\n") + player_line(session);
                          }
                      }
                      return text.empty() ? std::string{"no player"} : text;
                  });
    commands_.add("say", {{.name = "text", .rest = true}}, "Sends a message to every player",
                  [this](const core::CommandCall& call) {
                      const auto said = network_->say(call.text("text"));
                      if (const auto* const why = std::get_if<ChatRejection>(&said)) {
                          throw std::invalid_argument{
                              std::format("not sent: {} (1 to {} bytes, no control character)",
                                          to_string(*why), max_chat_size)};
                      }
                      return std::format("said to {} players", std::get<std::size_t>(said));
                  });
    commands_.add("close", {{.name = "room", .type = core::ArgType::Integer}},
                  "Ends the sessions of a room", [this](const core::CommandCall& call) {
                      const std::int64_t room = call.integer("room");
                      const auto rooms = network_->sessions().list_rooms();
                      const bool in_use =
                          std::ranges::any_of(rooms, [room](const RoomRecord& record) {
                              return std::cmp_equal(record.room, room);
                          });
                      if (!in_use) {
                          throw std::invalid_argument{std::format("room {} is not in use", room)};
                      }
                      const std::size_t ended =
                          network_->close_room(static_cast<std::uint32_t>(room));
                      return std::format("room {} closed: {} players logged out", room, ended);
                  });
    commands_.add("stop", {}, "Stops the server, telling the players",
                  [this](const core::CommandCall&) {
                      stop_();
                      return std::string{"stopping"};
                  });
}

core::CommandResult AdminConsole::run(std::string_view line) {
    return commands_.run(line);
}

struct ConsoleInput::Shared {
    mutable std::mutex mutex;
    std::vector<std::string> lines;
    bool ended = false;
};

ConsoleInput::ConsoleInput(std::istream& input) : shared_{std::make_shared<Shared>()} {
    std::thread{[shared = shared_, &input] {
        std::string line;
        while (std::getline(input, line)) {
            const std::scoped_lock lock{shared->mutex};
            shared->lines.push_back(line);
        }
        const std::scoped_lock lock{shared->mutex};
        shared->ended = true;
    }}.detach();
}

std::vector<std::string> ConsoleInput::take() {
    const std::scoped_lock lock{shared_->mutex};
    return std::exchange(shared_->lines, {});
}

bool ConsoleInput::ended() const {
    const std::scoped_lock lock{shared_->mutex};
    return shared_->ended;
}

} // namespace rtype
