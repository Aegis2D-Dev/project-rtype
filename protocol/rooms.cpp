#include <aegis2d/net/binary.hpp>

#include <rtype/login.hpp>
#include <rtype/rooms.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

namespace {

std::vector<std::byte> bytes_of(const net::BinaryWriter& writer) {
    return {writer.data().begin(), writer.data().end()};
}

// A message of one u8 after its kind: RoomCreated and RoomRejected.
std::optional<std::uint8_t> read_u8_message(std::span<const std::byte> message,
                                            TcpKind expected) noexcept {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<std::uint8_t> value = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(expected) || !value || !reader.at_end()) {
        return std::nullopt;
    }
    return value;
}

// A message that is its kind alone: ListRooms, LeaveRoom and GameStart.
bool is_kind_alone(std::span<const std::byte> message, TcpKind expected) noexcept {
    return message.size() == 1 && kind_of(message) == expected;
}

std::vector<std::byte> kind_alone(TcpKind kind) {
    return {std::byte{static_cast<std::uint8_t>(kind)}};
}

// A 0 or a 1, as the ready fields.
std::optional<bool> read_flag(net::BinaryReader& reader) noexcept {
    const std::optional<std::uint8_t> value = reader.read_u8();
    if (!value || *value > 1) {
        return std::nullopt;
    }
    return *value == 1;
}

std::vector<std::byte> write_u8_message(TcpKind kind, std::uint8_t value) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(kind));
    writer.write_u8(value);
    return bytes_of(writer);
}

} // namespace

std::string_view to_string(RoomRejection reason) noexcept {
    switch (reason) {
    case RoomRejection::NoRoomLeft:
        return "no room left";
    case RoomRejection::BadName:
        return "bad name";
    case RoomRejection::BadSize:
        return "bad size";
    case RoomRejection::BadLives:
        return "bad lives";
    case RoomRejection::BadDifficulty:
        return "bad difficulty";
    case RoomRejection::UnknownWaves:
        return "unknown waves";
    case RoomRejection::BadFriendlyFire:
        return "bad friendly fire";
    }
    return "unknown";
}

bool read_list_rooms(std::span<const std::byte> message) noexcept {
    return is_kind_alone(message, TcpKind::ListRooms);
}

std::optional<std::vector<RoomRecord>> read_room_list(std::span<const std::byte> message) {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<std::uint8_t> count = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::RoomList) || !count ||
        *count > max_listed_rooms) {
        return std::nullopt;
    }
    std::vector<RoomRecord> rooms;
    for (std::uint8_t index = 0; index < *count; ++index) {
        const std::optional<std::uint8_t> room = reader.read_u8();
        std::optional<std::string> name = reader.read_string();
        const std::optional<std::uint8_t> players = reader.read_u8();
        const std::optional<std::uint8_t> max_players = reader.read_u8();
        const std::optional<std::uint8_t> state = reader.read_u8();
        if (!room || !name || !players || !max_players || !state) {
            return std::nullopt;
        }
        rooms.push_back(RoomRecord{.room = *room,
                                   .name = std::move(*name),
                                   .players = *players,
                                   .max_players = *max_players,
                                   .state = *state});
    }
    if (!reader.at_end()) {
        return std::nullopt;
    }
    return rooms;
}

std::optional<CreateRoomRequest> read_create_room(std::span<const std::byte> message) {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    std::optional<std::string> name = reader.read_string();
    const std::optional<std::uint8_t> max_players = reader.read_u8();
    const std::optional<std::uint8_t> lives = reader.read_u8();
    const std::optional<std::uint8_t> difficulty = reader.read_u8();
    std::optional<std::string> waves = reader.read_string();
    const std::optional<std::uint8_t> friendly_fire = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::CreateRoom) || !name || !max_players || !lives ||
        !difficulty || !waves || !friendly_fire || !reader.at_end()) {
        return std::nullopt;
    }
    return CreateRoomRequest{.name = std::move(*name),
                             .max_players = *max_players,
                             .rules = RoomRules{.lives = *lives,
                                                .difficulty = *difficulty,
                                                .waves = std::move(*waves),
                                                .friendly_fire = *friendly_fire}};
}

std::optional<std::uint8_t> read_room_created(std::span<const std::byte> message) noexcept {
    return read_u8_message(message, TcpKind::RoomCreated);
}

std::optional<std::uint8_t> read_room_rejected(std::span<const std::byte> message) noexcept {
    return read_u8_message(message, TcpKind::RoomRejected);
}

std::vector<std::byte> write_list_rooms() {
    return kind_alone(TcpKind::ListRooms);
}

std::vector<std::byte> write_room_list(std::span<const RoomRecord> rooms) {
    if (rooms.size() > max_listed_rooms) {
        throw std::invalid_argument{"a RoomList holds at most 64 rooms"};
    }
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::RoomList));
    writer.write_u8(static_cast<std::uint8_t>(rooms.size()));
    for (const RoomRecord& room : rooms) {
        writer.write_u8(room.room);
        writer.write_string(room.name);
        writer.write_u8(room.players);
        writer.write_u8(room.max_players);
        writer.write_u8(room.state);
    }
    return bytes_of(writer);
}

std::vector<std::byte> write_create_room(const CreateRoomRequest& request) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::CreateRoom));
    writer.write_string(request.name);
    writer.write_u8(request.max_players);
    writer.write_u8(request.rules.lives);
    writer.write_u8(request.rules.difficulty);
    writer.write_string(request.rules.waves);
    writer.write_u8(request.rules.friendly_fire);
    return bytes_of(writer);
}

std::vector<std::byte> write_room_created(std::uint8_t room) {
    return write_u8_message(TcpKind::RoomCreated, room);
}

std::vector<std::byte> write_room_rejected(RoomRejection reason) {
    return write_u8_message(TcpKind::RoomRejected, static_cast<std::uint8_t>(reason));
}

std::string_view to_string(JoinRejection reason) noexcept {
    switch (reason) {
    case JoinRejection::NoSuchRoom:
        return "no such room";
    case JoinRejection::RoomFull:
        return "room full";
    case JoinRejection::InGame:
        return "in game";
    }
    return "unknown";
}

std::optional<std::uint8_t> read_join_room(std::span<const std::byte> message) noexcept {
    return read_u8_message(message, TcpKind::JoinRoom);
}

std::optional<std::pair<std::uint8_t, std::uint8_t>>
read_room_joined(std::span<const std::byte> message) noexcept {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<std::uint8_t> room = reader.read_u8();
    const std::optional<std::uint8_t> player = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::RoomJoined) || !room || !player ||
        !reader.at_end()) {
        return std::nullopt;
    }
    return std::pair{*room, *player};
}

std::optional<std::uint8_t> read_join_rejected(std::span<const std::byte> message) noexcept {
    return read_u8_message(message, TcpKind::JoinRejected);
}

bool read_leave_room(std::span<const std::byte> message) noexcept {
    return is_kind_alone(message, TcpKind::LeaveRoom);
}

std::optional<bool> read_set_ready(std::span<const std::byte> message) noexcept {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<bool> ready = read_flag(reader);
    if (kind != static_cast<std::uint8_t>(TcpKind::SetReady) || !ready || !reader.at_end()) {
        return std::nullopt;
    }
    return ready;
}

std::optional<RoomPlayers> read_room_players(std::span<const std::byte> message) {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<std::uint8_t> room = reader.read_u8();
    const std::optional<std::uint8_t> count = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::RoomPlayers) || !room || !count ||
        *count > tuning::max_players) {
        return std::nullopt;
    }
    RoomPlayers result{.room = *room, .players = {}};
    for (std::uint8_t index = 0; index < *count; ++index) {
        const std::optional<std::uint8_t> player = reader.read_u8();
        std::optional<std::string> name = reader.read_string();
        const std::optional<bool> ready = read_flag(reader);
        if (!player || !name || !ready) {
            return std::nullopt;
        }
        result.players.push_back(
            RoomPlayer{.player = *player, .name = std::move(*name), .ready = *ready});
    }
    if (!reader.at_end()) {
        return std::nullopt;
    }
    return result;
}

bool read_game_start(std::span<const std::byte> message) noexcept {
    return is_kind_alone(message, TcpKind::GameStart);
}

std::vector<std::byte> write_join_room(std::uint8_t room) {
    return write_u8_message(TcpKind::JoinRoom, room);
}

std::vector<std::byte> write_room_joined(std::uint8_t room, std::uint8_t player) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::RoomJoined));
    writer.write_u8(room);
    writer.write_u8(player);
    return bytes_of(writer);
}

std::vector<std::byte> write_join_rejected(JoinRejection reason) {
    return write_u8_message(TcpKind::JoinRejected, static_cast<std::uint8_t>(reason));
}

std::vector<std::byte> write_leave_room() {
    return kind_alone(TcpKind::LeaveRoom);
}

std::vector<std::byte> write_set_ready(bool ready) {
    return write_u8_message(TcpKind::SetReady, ready ? 1 : 0);
}

std::vector<std::byte> write_room_players(const RoomPlayers& players) {
    if (players.players.size() > tuning::max_players) {
        throw std::invalid_argument{"a RoomPlayers holds at most 4 players"};
    }
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::RoomPlayers));
    writer.write_u8(players.room);
    writer.write_u8(static_cast<std::uint8_t>(players.players.size()));
    for (const RoomPlayer& player : players.players) {
        writer.write_u8(player.player);
        writer.write_string(player.name);
        writer.write_u8(player.ready ? 1 : 0);
    }
    return bytes_of(writer);
}

std::vector<std::byte> write_game_start() {
    return kind_alone(TcpKind::GameStart);
}

} // namespace rtype
