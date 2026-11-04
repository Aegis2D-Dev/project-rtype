#include <catch2/catch_test_macros.hpp>
#include <rtype/input.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

using rtype::InputRecord;

namespace {

std::vector<std::byte> bytes(std::initializer_list<std::uint8_t> values) {
    std::vector<std::byte> out;
    for (const std::uint8_t value : values) {
        out.push_back(std::byte{value});
    }
    return out;
}

// Inputs from first to first + count - 1, each holding the low bits of its sequence.
std::vector<InputRecord> inputs(std::uint16_t first, std::size_t count) {
    std::vector<InputRecord> out;
    for (std::size_t index = 0; index < count; ++index) {
        const auto sequence = static_cast<std::uint16_t>(first + index);
        out.push_back(InputRecord{.sequence = sequence,
                                  .actions = static_cast<std::uint8_t>(sequence & 0x1FU)});
    }
    return out;
}

} // namespace

TEST_CASE("an input datagram carries the newest input and the 3 before it", "[protocol][input]") {
    // RFC section 4.2: input 0x0102 holds right and fire, the 3 before it right alone.
    const std::vector<InputRecord> sent{InputRecord{.sequence = 0x00FF, .actions = 0x08},
                                        InputRecord{.sequence = 0x0100, .actions = 0x08},
                                        InputRecord{.sequence = 0x0101, .actions = 0x08},
                                        InputRecord{.sequence = 0x0102, .actions = 0x18}};
    const std::vector<std::byte> payload = rtype::write_inputs(sent);
    CHECK(payload == bytes({0x02, 0x01, 0x04, 0x18, 0x08, 0x08, 0x08}));
    CHECK(payload.size() == rtype::input_payload_size(rtype::max_inputs_per_datagram));
    CHECK(rtype::read_inputs(payload) == std::optional{sent});

    // The first inputs of a session go with fewer before them; sequences wrap.
    for (std::size_t count = 1; count <= rtype::max_inputs_per_datagram; ++count) {
        CAPTURE(count);
        CHECK(rtype::write_inputs(inputs(0, count)).size() == rtype::input_payload_size(count));
        CHECK(rtype::read_inputs(rtype::write_inputs(inputs(0, count))) ==
              std::optional{inputs(0, count)});
        CHECK(rtype::read_inputs(rtype::write_inputs(inputs(65534, count))) ==
              std::optional{inputs(65534, count)});
    }
}

TEST_CASE("bits outside the actions are never sent", "[protocol][input]") {
    const std::vector<std::byte> payload =
        rtype::write_inputs(std::vector{InputRecord{.sequence = 7, .actions = 0xE8}});
    CHECK(payload == bytes({0x07, 0x00, 0x01, 0x08}));
}

TEST_CASE("only 1 to 4 consecutive inputs make an input datagram", "[protocol][input]") {
    CHECK_THROWS_AS(rtype::write_inputs({}), std::invalid_argument);
    CHECK_THROWS_AS(rtype::write_inputs(inputs(0, 5)), std::invalid_argument);
    std::vector<InputRecord> gap = inputs(0, 3);
    gap.back().sequence = 3;
    CHECK_THROWS_AS(rtype::write_inputs(gap), std::invalid_argument);
    std::vector<InputRecord> newest_first = inputs(0, 2);
    std::swap(newest_first.front(), newest_first.back());
    CHECK_THROWS_AS(rtype::write_inputs(newest_first), std::invalid_argument);
}

TEST_CASE("malformed input datagrams are refused", "[protocol][input]") {
    CHECK(!rtype::read_inputs({}));
    CHECK(!rtype::read_inputs(bytes({0x07})));
    CHECK(!rtype::read_inputs(bytes({0x07, 0x00})));
    CHECK(!rtype::read_inputs(bytes({0x07, 0x00, 0x00})));                // no input
    CHECK(!rtype::read_inputs(bytes({0x07, 0x00, 0x05, 0, 0, 0, 0, 0}))); // 5 inputs
    CHECK(!rtype::read_inputs(bytes({0x07, 0x00, 0x02, 0x08})));          // one short
    CHECK(!rtype::read_inputs(bytes({0x07, 0x00, 0x01, 0x08, 0x08})));    // one over
    // The 3-byte Input of before the redundant inputs: its actions read as a count.
    CHECK(!rtype::read_inputs(bytes({0x07, 0x00, 0x08})));
    for (unsigned bit = 5; bit < 8; ++bit) {
        const auto set = static_cast<std::uint8_t>(1U << bit);
        CHECK(!rtype::read_inputs(bytes({0x07, 0x00, 0x01, set})));
        CHECK(!rtype::read_inputs(bytes({0x07, 0x00, 0x03, 0x08, 0x08, set})));
    }
}
