#include <aegis2d/net/binary.hpp>

#include <rtype/input.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

std::vector<std::byte> write_inputs(std::span<const InputRecord> inputs) {
    if (inputs.empty() || inputs.size() > max_inputs_per_datagram) {
        throw std::invalid_argument{"rtype: an Input carries 1 to 4 inputs"};
    }
    std::uint16_t expected = inputs.front().sequence;
    for (const InputRecord& input : inputs) {
        if (input.sequence != expected) {
            throw std::invalid_argument{"rtype: the inputs of an Input follow each other"};
        }
        expected = static_cast<std::uint16_t>(expected + 1U);
    }
    net::BinaryWriter writer;
    writer.write_u16(inputs.back().sequence);
    writer.write_u8(static_cast<std::uint8_t>(inputs.size()));
    for (const InputRecord& input : std::views::reverse(inputs)) {
        writer.write_u8(static_cast<std::uint8_t>(input.actions & valid_actions));
    }
    return {writer.data().begin(), writer.data().end()};
}

std::optional<std::vector<InputRecord>> read_inputs(std::span<const std::byte> payload) {
    net::BinaryReader reader{payload};
    const std::optional<std::uint16_t> newest = reader.read_u16();
    const std::optional<std::uint8_t> count = reader.read_u8();
    if (!newest || !count || *count == 0 || *count > max_inputs_per_datagram ||
        payload.size() != input_payload_size(*count)) {
        return std::nullopt;
    }
    // Newest first on the wire, oldest first in the result.
    std::vector<InputRecord> inputs(*count);
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        const std::uint8_t actions = reader.read_u8().value_or(0xFFU);
        if ((actions & ~valid_actions) != 0U) {
            return std::nullopt;
        }
        inputs.at(inputs.size() - 1 - index) = InputRecord{
            .sequence = static_cast<std::uint16_t>(*newest - index), .actions = actions};
    }
    return inputs;
}

} // namespace rtype
