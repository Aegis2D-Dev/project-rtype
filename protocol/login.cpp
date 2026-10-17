#include <aegis2d/net/binary.hpp>

#include <rtype/login.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

namespace {

// Kinds are contiguous: checked as a range before the cast, like net::PacketType.
std::optional<TcpKind> tcp_kind(std::uint8_t value) noexcept {
    if (value < static_cast<std::uint8_t>(TcpKind::LoginRequest) ||
        value > static_cast<std::uint8_t>(TcpKind::Logout)) {
        return std::nullopt;
    }
    return static_cast<TcpKind>(value);
}

// Bytes of the UTF-8 sequence starting with lead, and the smallest code point it may hold
// (smaller ones are overlong forms); 0 for a byte that cannot start a sequence.
struct Lead {
    std::size_t size = 0;
    char32_t value = 0;
    char32_t min = 0;
};

Lead lead_of(unsigned char byte) noexcept {
    if (byte < 0x80U) {
        return {.size = 1, .value = byte, .min = 0};
    }
    if ((byte & 0xE0U) == 0xC0U) {
        return {.size = 2, .value = byte & 0x1FU, .min = 0x80};
    }
    if ((byte & 0xF0U) == 0xE0U) {
        return {.size = 3, .value = byte & 0x0FU, .min = 0x800};
    }
    if ((byte & 0xF8U) == 0xF0U) {
        return {.size = 4, .value = byte & 0x07U, .min = 0x10000};
    }
    return {};
}

bool is_control(char32_t code_point) noexcept {
    return code_point < 0x20U || code_point == 0x7FU;
}

} // namespace

std::string_view to_string(LoginRejection reason) noexcept {
    switch (reason) {
    case LoginRejection::ServerFull:
        return "server full";
    case LoginRejection::BadVersion:
        return "bad version";
    case LoginRejection::BadName:
        return "bad name";
    case LoginRejection::NameTaken:
        return "name taken";
    case LoginRejection::GameStarted:
        return "game started";
    }
    return "unknown";
}

bool valid_player_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > max_name_size) {
        return false;
    }
    for (std::size_t start = 0; start < name.size();) {
        const Lead lead = lead_of(static_cast<unsigned char>(name.at(start)));
        if (lead.size == 0 || name.size() - start < lead.size) {
            return false;
        }
        char32_t code_point = lead.value;
        for (std::size_t index = 1; index < lead.size; ++index) {
            const auto byte = static_cast<unsigned char>(name.at(start + index));
            if ((byte & 0xC0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (byte & 0x3FU);
        }
        start += lead.size;
        if (code_point < lead.min || code_point > 0x10FFFFU ||
            (code_point >= 0xD800U && code_point <= 0xDFFFU) || is_control(code_point)) {
            return false;
        }
    }
    return true;
}

std::optional<TcpKind> kind_of(std::span<const std::byte> message) noexcept {
    if (message.empty()) {
        return std::nullopt;
    }
    return tcp_kind(std::to_integer<std::uint8_t>(message.front()));
}

std::optional<LoginRequest> read_login_request(std::span<const std::byte> message) {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::LoginRequest)) {
        return std::nullopt;
    }
    const std::optional<std::uint8_t> version = reader.read_u8();
    std::optional<std::string> name = reader.read_string();
    if (!version || !name || !reader.at_end()) {
        return std::nullopt;
    }
    return LoginRequest{.version = *version, .name = std::move(*name)};
}

std::optional<LoginAccepted> read_login_accepted(std::span<const std::byte> message) noexcept {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<std::uint32_t> session = reader.read_u32();
    const std::optional<std::uint8_t> player = reader.read_u8();
    const std::optional<std::uint16_t> udp_port = reader.read_u16();
    if (kind != static_cast<std::uint8_t>(TcpKind::LoginAccepted) || !session || !player ||
        !udp_port || !reader.at_end()) {
        return std::nullopt;
    }
    return LoginAccepted{.session = *session, .player = *player, .udp_port = *udp_port};
}

std::optional<std::uint8_t> read_login_rejected(std::span<const std::byte> message) noexcept {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<std::uint8_t> reason = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::LoginRejected) || !reason || !reader.at_end()) {
        return std::nullopt;
    }
    return reason;
}

std::optional<std::uint8_t> read_logout(std::span<const std::byte> message) noexcept {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<std::uint8_t> reason = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::Logout) || !reason || !reader.at_end()) {
        return std::nullopt;
    }
    return reason;
}

std::vector<std::byte> write_login_request(const LoginRequest& request) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::LoginRequest));
    writer.write_u8(request.version);
    writer.write_string(request.name);
    return {writer.data().begin(), writer.data().end()};
}

std::vector<std::byte> write_login_accepted(const LoginAccepted& accepted) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::LoginAccepted));
    writer.write_u32(accepted.session);
    writer.write_u8(accepted.player);
    writer.write_u16(accepted.udp_port);
    return {writer.data().begin(), writer.data().end()};
}

std::vector<std::byte> write_login_rejected(LoginRejection reason) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::LoginRejected));
    writer.write_u8(static_cast<std::uint8_t>(reason));
    return {writer.data().begin(), writer.data().end()};
}

std::vector<std::byte> write_logout(LogoutReason reason) {
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::Logout));
    writer.write_u8(static_cast<std::uint8_t>(reason));
    return {writer.data().begin(), writer.data().end()};
}

} // namespace rtype
