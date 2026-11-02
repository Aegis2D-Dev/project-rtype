/// @file
/// @brief Compact encoders of snapshot fields: fixed-point positions, integer velocities and
/// varint ids, written bit by bit (RFC section 4.3).
#pragma once
#include <aegis2d/net/bits.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rtype {

/// @brief Steps of a position per logical pixel: positions are sent to the 1/8 px.
inline constexpr std::int32_t position_scale = 8;
/// @brief Bits of a position, sign included.
inline constexpr unsigned position_bits = 15;
/// @brief Smallest position sent, in logical pixels: -2048.
/// @details The play area (0 to tuning::area_width, 0 to tuning::area_height) and the
/// entities just beyond its edges lie well within [position_min, position_max].
inline constexpr float position_min =
    -static_cast<float>(std::int32_t{1} << (position_bits - 1U)) / position_scale;
/// @brief Largest position sent, in logical pixels: 2047.875.
inline constexpr float position_max =
    static_cast<float>((std::int32_t{1} << (position_bits - 1U)) - 1) / position_scale;
/// @brief Largest error of a position within [position_min, position_max]: 1/16 px.
inline constexpr float position_max_error = 0.5F / position_scale;

/// @brief Bits of a velocity, sign included; velocities are sent in whole pixels per second.
inline constexpr unsigned velocity_bits = 12;
/// @brief Smallest velocity sent, in logical pixels per second: -2048.
inline constexpr float velocity_min = -static_cast<float>(std::int32_t{1} << (velocity_bits - 1U));
/// @brief Largest velocity sent, in logical pixels per second: 2047.
inline constexpr float velocity_max =
    static_cast<float>((std::int32_t{1} << (velocity_bits - 1U)) - 1);
/// @brief Largest error of a velocity within [velocity_min, velocity_max]: 0.5 px/s.
inline constexpr float velocity_max_error = 0.5F;

/// @brief Bits of each group of a varint, followed by one continuation bit.
inline constexpr unsigned varint_group_bits = 7;
/// @brief Most groups of a varint: 5, for 32 bits.
inline constexpr std::size_t varint_max_groups = 5;

/// @brief Rounds a position to the nearest 1/8 px, clamped to [position_min, position_max].
/// @param position Position in logical pixels; NaN gives 0.
/// @return The position in steps, from -2^14 to 2^14 - 1.
[[nodiscard]] std::int32_t quantize_position(float position) noexcept;
/// @brief Position of a number of steps.
/// @param steps Position in 1/8 px.
/// @return The position in logical pixels, exact.
[[nodiscard]] float dequantize_position(std::int32_t steps) noexcept;
/// @brief Rounds a velocity to the nearest whole pixel per second, clamped to
/// [velocity_min, velocity_max].
/// @param velocity Velocity in logical pixels per second; NaN gives 0.
/// @return The velocity, from -2^11 to 2^11 - 1.
[[nodiscard]] std::int32_t quantize_velocity(float velocity) noexcept;

/// @brief Writes a position on position_bits bits, see quantize_position().
/// @param writer Destination.
/// @param position Position in logical pixels.
void write_position(aegis2d::net::BitWriter& writer, float position);
/// @brief Reads a position written by write_position().
/// @param reader Source.
/// @return The position in logical pixels, or nothing if the reader has failed or too few
/// bits remain.
[[nodiscard]] std::optional<float> read_position(aegis2d::net::BitReader& reader) noexcept;
/// @brief Writes a velocity on velocity_bits bits, see quantize_velocity().
/// @param writer Destination.
/// @param velocity Velocity in logical pixels per second.
void write_velocity(aegis2d::net::BitWriter& writer, float velocity);
/// @brief Reads a velocity written by write_velocity().
/// @param reader Source.
/// @return The velocity in logical pixels per second, or nothing if the reader has failed or
/// too few bits remain.
[[nodiscard]] std::optional<float> read_velocity(aegis2d::net::BitReader& reader) noexcept;

/// @brief Writes an id as a varint: groups of varint_group_bits bits, lowest first, each
/// followed by a continuation bit set when another group follows.
/// @details An id below 128 takes 8 bits, below 16384 16 bits, and 40 bits at most.
/// @param writer Destination.
/// @param value Value to write.
void write_varint(aegis2d::net::BitWriter& writer, std::uint32_t value);
/// @brief Reads a varint written by write_varint().
/// @details A varint has a single encoding: one with a last group of 0 bits (other than a
/// lone first group), more than varint_max_groups groups or a value above 32 bits is
/// rejected.
/// @param reader Source; it fails on a rejected varint.
/// @return The value, or nothing if the varint is truncated or rejected.
[[nodiscard]] std::optional<std::uint32_t> read_varint(aegis2d::net::BitReader& reader) noexcept;
/// @brief Size of a varint.
/// @param value Value to write.
/// @return The number of bits write_varint() takes for @p value.
[[nodiscard]] std::size_t varint_bits(std::uint32_t value) noexcept;

} // namespace rtype
