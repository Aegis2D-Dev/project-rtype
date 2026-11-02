#include <aegis2d/net/bits.hpp>

#include <rtype/compact.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace rtype {

namespace net = aegis2d::net;

namespace {

// Rounds half away from zero and clamps to [low, high]; NaN gives 0.
std::int32_t round_clamped(float value, float low, float high) noexcept {
    if (std::isnan(value)) {
        return 0;
    }
    return static_cast<std::int32_t>(std::round(std::fmin(std::fmax(value, low), high)));
}

// A width above BitWriter::max_width always fails, and the failure sticks.
void fail(net::BitReader& reader) noexcept {
    static_cast<void>(reader.read_bits(net::BitWriter::max_width + 1U));
}

constexpr std::uint32_t group_mask = (std::uint32_t{1} << varint_group_bits) - 1U;

} // namespace

std::int32_t quantize_position(float position) noexcept {
    constexpr auto scale = static_cast<float>(position_scale);
    return round_clamped(position * scale, position_min * scale, position_max * scale);
}

float dequantize_position(std::int32_t steps) noexcept {
    return static_cast<float>(steps) / static_cast<float>(position_scale);
}

std::int32_t quantize_velocity(float velocity) noexcept {
    return round_clamped(velocity, velocity_min, velocity_max);
}

void write_position(net::BitWriter& writer, float position) {
    writer.write_signed(quantize_position(position), position_bits);
}

std::optional<float> read_position(net::BitReader& reader) noexcept {
    const std::optional<std::int64_t> steps = reader.read_signed(position_bits);
    if (!steps) {
        return std::nullopt;
    }
    return dequantize_position(static_cast<std::int32_t>(*steps));
}

void write_velocity(net::BitWriter& writer, float velocity) {
    writer.write_signed(quantize_velocity(velocity), velocity_bits);
}

std::optional<float> read_velocity(net::BitReader& reader) noexcept {
    const std::optional<std::int64_t> velocity = reader.read_signed(velocity_bits);
    if (!velocity) {
        return std::nullopt;
    }
    return static_cast<float>(*velocity);
}

void write_varint(net::BitWriter& writer, std::uint32_t value) {
    for (bool more = true; more;) {
        const std::uint32_t group = value & group_mask;
        value >>= varint_group_bits;
        more = value != 0U;
        writer.write_bits(group, varint_group_bits);
        writer.write_bool(more);
    }
}

std::optional<std::uint32_t> read_varint(net::BitReader& reader) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < varint_max_groups; ++index) {
        const std::optional<std::uint64_t> group = reader.read_bits(varint_group_bits);
        const std::optional<bool> more = reader.read_bool();
        if (!group || !more) {
            return std::nullopt;
        }
        value |= *group << (index * varint_group_bits);
        if (!*more) {
            // A last group of 0 has a shorter encoding; only 0 itself is a lone group of 0.
            if ((*group == 0U && index > 0U) || value > std::numeric_limits<std::uint32_t>::max()) {
                fail(reader);
                return std::nullopt;
            }
            return static_cast<std::uint32_t>(value);
        }
    }
    fail(reader);
    return std::nullopt;
}

std::size_t varint_bits(std::uint32_t value) noexcept {
    std::size_t groups = 1;
    while ((value >>= varint_group_bits) != 0U) {
        ++groups;
    }
    return groups * (varint_group_bits + 1U);
}

} // namespace rtype
