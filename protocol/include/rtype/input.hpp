/// @file
/// @brief Inputs of a player sent to the server (RFC section 4.2).
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtype {

/// @brief Bits of the actions of an `Input`: up, down, left, right and fire; bits 5 to 7
/// must be clear.
inline constexpr std::uint8_t valid_actions = 0x1FU;

/// @brief Size of the payload of an `Input` datagram: its sequence and its actions.
inline constexpr std::size_t input_payload_size = 3;

/// @brief What an `Input` datagram says: the actions held at one tick of the client.
struct InputRecord {
    /// @brief Number of the input: 0 for the first `Input` of a session, then one more for
    /// each, from 65535 back to 0. Compared with `aegis2d::net::sequence_greater_than()`.
    std::uint16_t sequence = 0;
    /// @brief Actions held, one `aegis2d::ecs::Action` bit each.
    std::uint8_t actions = 0;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const InputRecord&) const = default;
};

/// @brief Encodes the payload of an `Input` datagram, header excluded.
/// @param input Sequence and actions; actions outside valid_actions are cleared.
/// @return The input_payload_size bytes: the sequence (`u16`), then the actions (`u8`).
[[nodiscard]] std::vector<std::byte> write_input(const InputRecord& input);

/// @brief Decodes the payload of an `Input` datagram.
/// @param payload Bytes after the header.
/// @return The input, or `std::nullopt` if the payload is not input_payload_size bytes or
/// sets a bit outside valid_actions.
[[nodiscard]] std::optional<InputRecord> read_input(std::span<const std::byte> payload);

} // namespace rtype
