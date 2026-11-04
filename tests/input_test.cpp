#include <catch2/catch_test_macros.hpp>
#include <rtype/input.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
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

} // namespace

TEST_CASE("an input is its sequence then its actions", "[protocol][input]") {
    // RFC section 4.2: input 0x0102, holding right and fire.
    const InputRecord input{.sequence = 0x0102, .actions = 0x18};
    const std::vector<std::byte> payload = rtype::write_input(input);
    CHECK(payload == bytes({0x02, 0x01, 0x18}));
    CHECK(payload.size() == rtype::input_payload_size);
    CHECK(rtype::read_input(payload) == std::optional{input});

    // Every sequence, the first and the last before the wrap included.
    for (const std::uint16_t sequence : {0, 1, 255, 256, 32768, 65535}) {
        const InputRecord any{.sequence = sequence, .actions = rtype::valid_actions};
        CHECK(rtype::read_input(rtype::write_input(any)) == std::optional{any});
    }
}

TEST_CASE("bits outside the actions are never sent", "[protocol][input]") {
    const std::vector<std::byte> payload =
        rtype::write_input(InputRecord{.sequence = 7, .actions = 0xE8});
    CHECK(payload == bytes({0x07, 0x00, 0x08}));
}

TEST_CASE("malformed inputs are refused", "[protocol][input]") {
    CHECK(!rtype::read_input({}));
    CHECK(!rtype::read_input(bytes({0x07})));
    CHECK(!rtype::read_input(bytes({0x07, 0x00})));
    CHECK(!rtype::read_input(bytes({0x07, 0x00, 0x08, 0x00})));
    // The 1-byte Input of before the sequence.
    CHECK(!rtype::read_input(bytes({0x08})));
    for (unsigned bit = 5; bit < 8; ++bit) {
        CHECK(!rtype::read_input(bytes({0x07, 0x00, static_cast<std::uint8_t>(1U << bit)})));
    }
}
