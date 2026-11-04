#include <aegis2d/net/binary.hpp>

#include <rtype/input.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

std::vector<std::byte> write_input(const InputRecord& input) {
    net::BinaryWriter writer;
    writer.write_u16(input.sequence);
    writer.write_u8(static_cast<std::uint8_t>(input.actions & valid_actions));
    return {writer.data().begin(), writer.data().end()};
}

std::optional<InputRecord> read_input(std::span<const std::byte> payload) {
    net::BinaryReader reader{payload};
    const std::optional<std::uint16_t> sequence = reader.read_u16();
    const std::optional<std::uint8_t> actions = reader.read_u8();
    if (!sequence || !actions || !reader.at_end() || (*actions & ~valid_actions) != 0U) {
        return std::nullopt;
    }
    return InputRecord{.sequence = *sequence, .actions = *actions};
}

} // namespace rtype
