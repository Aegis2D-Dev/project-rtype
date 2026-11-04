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

/// @brief Inputs an `Input` datagram carries at most: the newest and the 3 before it, so a
/// lost datagram's input arrives with one of the next three.
inline constexpr std::size_t max_inputs_per_datagram = 4;

/// @brief Size of the payload of an `Input` datagram carrying @p count inputs: the sequence
/// of the newest, the count, then the actions of each.
/// @param count Number of inputs, 1 to max_inputs_per_datagram.
/// @return `3 + count` bytes.
[[nodiscard]] constexpr std::size_t input_payload_size(std::size_t count) noexcept {
    return 3 + count;
}

/// @brief One input of the player: the actions held at one tick of the client.
struct InputRecord {
    /// @brief Number of the input: 0 for the first input of a session, then one more for
    /// each, from 65535 back to 0. Compared with `aegis2d::net::sequence_greater_than()`.
    std::uint16_t sequence = 0;
    /// @brief Actions held, one `aegis2d::ecs::Action` bit each.
    std::uint8_t actions = 0;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const InputRecord&) const = default;
};

/// @brief Encodes the payload of an `Input` datagram, header excluded.
/// @details The sequence of the newest input (`u16`), the number of inputs (`u8`), then the
/// actions of each (`u8`), newest first; actions outside valid_actions are cleared.
/// @param inputs 1 to max_inputs_per_datagram inputs of consecutive sequences, oldest first.
/// @return The input_payload_size() bytes.
/// @throws std::invalid_argument if @p inputs is empty, too long, or not consecutive.
[[nodiscard]] std::vector<std::byte> write_inputs(std::span<const InputRecord> inputs);

/// @brief Decodes the payload of an `Input` datagram.
/// @param payload Bytes after the header.
/// @return Its inputs, oldest first, their sequences consecutive and the last one the
/// newest's; or `std::nullopt` if the count is 0 or above max_inputs_per_datagram, the
/// payload is not input_payload_size() bytes for it, or an input sets a bit outside
/// valid_actions.
[[nodiscard]] std::optional<std::vector<InputRecord>>
read_inputs(std::span<const std::byte> payload);

} // namespace rtype
