#include <aegis2d/net/bits.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/compact.hpp>
#include <rtype/tuning.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

using aegis2d::net::BitReader;
using aegis2d::net::BitWriter;
namespace tuning = rtype::tuning;

namespace {

/// Writes then reads @p position.
std::optional<float> position_round_trip(float position) {
    BitWriter writer;
    rtype::write_position(writer, position);
    CHECK(writer.bit_size() == rtype::position_bits);
    BitReader reader{writer.data()};
    const std::optional<float> read = rtype::read_position(reader);
    CHECK(reader.at_end());
    return read;
}

/// Writes then reads @p velocity.
std::optional<float> velocity_round_trip(float velocity) {
    BitWriter writer;
    rtype::write_velocity(writer, velocity);
    CHECK(writer.bit_size() == rtype::velocity_bits);
    BitReader reader{writer.data()};
    const std::optional<float> read = rtype::read_velocity(reader);
    CHECK(reader.at_end());
    return read;
}

/// Writes @p groups of 7 bits, each with its continuation bit.
std::vector<std::byte> varint_groups(std::initializer_list<std::uint64_t> groups) {
    BitWriter writer;
    std::size_t left = groups.size();
    for (const std::uint64_t group : groups) {
        writer.write_bits(group, rtype::varint_group_bits);
        writer.write_bool(--left != 0U);
    }
    return {writer.data().begin(), writer.data().end()};
}

} // namespace

TEST_CASE("positions cover the play area and the entities beyond its edges", "[compact]") {
    STATIC_REQUIRE(rtype::position_min == -2048.0F);
    STATIC_REQUIRE(rtype::position_max == 2047.875F);
    STATIC_REQUIRE(rtype::position_max_error == 0.0625F);
    // The farthest entities: a Bydos spawning beyond the right edge, a missile leaving.
    CHECK(rtype::position_min < -tuning::player_missile_size.x);
    CHECK(rtype::position_max > tuning::bydos_spawn_x + tuning::bydos_size.x);
    CHECK(rtype::position_max > tuning::area_width + tuning::area_width / 2.0F);
}

TEST_CASE("positions on the 1/8 px grid make the round trip exactly", "[compact]") {
    for (const float position :
         {0.0F, 0.125F, -0.125F, tuning::area_width, tuning::area_height, tuning::bydos_spawn_x,
          tuning::player_spawn_x, rtype::position_min, rtype::position_max}) {
        CHECK(position_round_trip(position) == position);
    }
}

TEST_CASE("positions within the range are off by 1/16 px at most", "[compact]") {
    // Every 1/100 px across the play area and its margins, each edge included.
    for (int hundredths = -10000; hundredths <= 150000; ++hundredths) {
        const float position = static_cast<float>(hundredths) / 100.0F;
        const float sent = rtype::dequantize_position(rtype::quantize_position(position));
        REQUIRE(std::fabs(sent - position) <= rtype::position_max_error);
    }
    for (const float edge : {0.0F, tuning::area_width, tuning::area_height}) {
        for (const float offset : {-0.0624F, -0.0001F, 0.0001F, 0.0624F}) {
            CHECK(position_round_trip(edge + offset) == edge);
        }
        // Halfway between two steps, the farther from zero is taken.
        CHECK(position_round_trip(edge + 0.0625F) == edge + 0.125F);
    }
}

TEST_CASE("positions out of the range are clamped to its ends", "[compact]") {
    CHECK(position_round_trip(rtype::position_max + 0.0625F) == rtype::position_max);
    CHECK(position_round_trip(5000.0F) == rtype::position_max);
    CHECK(position_round_trip(-5000.0F) == rtype::position_min);
    CHECK(position_round_trip(std::numeric_limits<float>::infinity()) == rtype::position_max);
    CHECK(position_round_trip(-std::numeric_limits<float>::infinity()) == rtype::position_min);
    CHECK(position_round_trip(std::numeric_limits<float>::quiet_NaN()) == 0.0F);
}

TEST_CASE("velocities are sent in whole pixels per second", "[compact]") {
    STATIC_REQUIRE(rtype::velocity_min == -2048.0F);
    STATIC_REQUIRE(rtype::velocity_max == 2047.0F);
    for (const float speed : {tuning::player_speed, tuning::player_missile_speed,
                              tuning::bydos_speed, tuning::enemy_missile_speed}) {
        CHECK(velocity_round_trip(speed) == speed);
        CHECK(velocity_round_trip(-speed) == -speed);
    }
    // A diagonal player ship: 212.13 px/s on each axis.
    const float diagonal = tuning::player_speed / std::numbers::sqrt2_v<float>;
    const float read = velocity_round_trip(diagonal).value_or(-1.0F);
    CHECK(read == 212.0F);
    CHECK(std::fabs(read - diagonal) <= rtype::velocity_max_error);
    CHECK(velocity_round_trip(0.5F) == 1.0F);
    CHECK(velocity_round_trip(-0.49F) == 0.0F);
}

TEST_CASE("velocities out of the range are clamped to its ends", "[compact]") {
    CHECK(velocity_round_trip(2047.4F) == rtype::velocity_max);
    CHECK(velocity_round_trip(9000.0F) == rtype::velocity_max);
    CHECK(velocity_round_trip(-9000.0F) == rtype::velocity_min);
    CHECK(velocity_round_trip(std::numeric_limits<float>::quiet_NaN()) == 0.0F);
}

TEST_CASE("a truncated position or velocity is not read", "[compact]") {
    // One byte: 8 bits, fewer than either field.
    const std::vector<std::byte> byte{std::byte{0xFF}};
    BitReader position{byte};
    CHECK_FALSE(rtype::read_position(position));
    CHECK(position.failed());
    BitReader velocity{byte};
    CHECK_FALSE(rtype::read_velocity(velocity));
    CHECK(velocity.failed());
}

TEST_CASE("varints make the round trip with the size of their value", "[compact]") {
    struct Case {
        std::uint32_t value;
        std::size_t bits;
    };
    for (const Case& test :
         {Case{.value = 0, .bits = 8}, Case{.value = 1, .bits = 8}, Case{.value = 127, .bits = 8},
          Case{.value = 128, .bits = 16}, Case{.value = 16383, .bits = 16},
          Case{.value = 16384, .bits = 24}, Case{.value = 2097151, .bits = 24},
          Case{.value = 2097152, .bits = 32}, Case{.value = 268435455, .bits = 32},
          Case{.value = 268435456, .bits = 40},
          Case{.value = std::numeric_limits<std::uint32_t>::max(), .bits = 40}}) {
        BitWriter writer;
        rtype::write_varint(writer, test.value);
        CHECK(writer.bit_size() == test.bits);
        CHECK(rtype::varint_bits(test.value) == test.bits);
        BitReader reader{writer.data()};
        CHECK(rtype::read_varint(reader) == test.value);
        CHECK(reader.at_end());
    }
}

TEST_CASE("varints follow each other at any bit offset", "[compact]") {
    BitWriter writer;
    writer.write_bits(5, 3);
    rtype::write_varint(writer, 300);
    rtype::write_position(writer, 640.5F);
    rtype::write_varint(writer, 7);
    BitReader reader{writer.data()};
    CHECK(reader.read_bits(3) == 5U);
    CHECK(rtype::read_varint(reader) == 300U);
    CHECK(rtype::read_position(reader) == 640.5F);
    CHECK(rtype::read_varint(reader) == 7U);
    CHECK(reader.at_end());
}

TEST_CASE("a varint with another encoding than write_varint's is rejected", "[compact]") {
    // 1 with a last group of 0: 1 has a single group.
    const std::vector<std::byte> trailing_zero = varint_groups({1, 0});
    // Six groups.
    const std::vector<std::byte> too_long = varint_groups({1, 1, 1, 1, 1, 1});
    // 2^32: five groups, the last above the 4 bits that remain.
    const std::vector<std::byte> too_large = varint_groups({0, 0, 0, 0, 16});
    for (const auto* bytes : {&trailing_zero, &too_long, &too_large}) {
        BitReader reader{*bytes};
        CHECK_FALSE(rtype::read_varint(reader));
        CHECK(reader.failed());
        CHECK_FALSE(reader.read_bits(0));
    }
}

TEST_CASE("a truncated varint is not read", "[compact]") {
    BitWriter writer;
    rtype::write_varint(writer, 300);
    for (std::size_t bits = 0; bits < writer.bit_size(); ++bits) {
        // The first bits of the varint, after padding: the reader has exactly those bits.
        const auto padding = static_cast<unsigned>((8 - (bits % 8)) % 8);
        BitWriter cut;
        cut.write_bits(0, padding);
        BitReader full{writer.data()};
        for (std::size_t bit = 0; bit < bits; ++bit) {
            cut.write_bool(full.read_bool().value_or(false));
        }
        BitReader reader{cut.data()};
        REQUIRE(reader.read_bits(padding));
        CHECK_FALSE(rtype::read_varint(reader));
        CHECK(reader.failed());
    }
}
