#include <aegis2d/net/binary.hpp>

#include <rtype/login.hpp>
#include <rtype/rooms.hpp>

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
    }
    return "unknown";
}

bool read_list_rooms(std::span<const std::byte> message) noexcept {
    return message.size() == 1 && kind_of(message) == TcpKind::ListRooms;
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
    if (kind != static_cast<std::uint8_t>(TcpKind::CreateRoom) || !name || !max_players ||
        !reader.at_end()) {
        return std::nullopt;
    }
    return CreateRoomRequest{.name = std::move(*name), .max_players = *max_players};
}

std::optional<std::uint8_t> read_room_created(std::span<const std::byte> message) noexcept {
    return read_u8_message(message, TcpKind::RoomCreated);
}

std::optional<std::uint8_t> read_room_rejected(std::span<const std::byte> message) noexcept {
    return read_u8_message(message, TcpKind::RoomRejected);
}

std::vector<std::byte> write_list_rooms() {
    return {std::byte{static_cast<std::uint8_t>(TcpKind::ListRooms)}};
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
    return bytes_of(writer);
}

std::vector<std::byte> write_room_created(std::uint8_t room) {
    return write_u8_message(TcpKind::RoomCreated, room);
}

std::vector<std::byte> write_room_rejected(RoomRejection reason) {
    return write_u8_message(TcpKind::RoomRejected, static_cast<std::uint8_t>(reason));
}

} // namespace rtype
