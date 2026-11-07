#include <catch2/catch_test_macros.hpp>
#include <rtype/login.hpp>
#include <rtype/scores.hpp>

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

using rtype::ScoreRecord;

namespace {

std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for (const unsigned value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

} // namespace

TEST_CASE("scoreboard messages follow the RFC example", "[protocol][scores]") {
    // RFC section 3.9, without the length prefixes.
    CHECK(rtype::write_list_scores() == bytes({0x14}));
    CHECK(rtype::read_list_scores(bytes({0x14})));
    CHECK(rtype::kind_of(bytes({0x14})) == rtype::TcpKind::ListScores);

    const std::vector<std::byte> list =
        bytes({0x15, 0x02, 0x03, 0x00, 0x4B, 0x61, 0x7A, 0xB0, 0x04, 0x00, 0x00, 0x02, 0x00, 0x52,
               0x39, 0x2C, 0x01, 0x00, 0x00});
    const std::vector<ScoreRecord> scores{{.name = "Kaz", .score = 1200},
                                          {.name = "R9", .score = 300}};
    CHECK(rtype::write_score_list(scores) == list);
    CHECK(rtype::read_score_list(list) == scores);
    CHECK(rtype::kind_of(list) == rtype::TcpKind::ScoreList);
    CHECK(!rtype::kind_of(bytes({0x16})).has_value());
}

TEST_CASE("malformed scoreboard messages are refused", "[protocol][scores]") {
    CHECK(!rtype::read_list_scores(bytes({0x14, 0x00})));
    CHECK(!rtype::read_list_scores(bytes({0x05})));
    CHECK(rtype::read_score_list(bytes({0x15, 0x00})) == std::vector<ScoreRecord>{});
    CHECK(!rtype::read_score_list(bytes({0x15})).has_value());
    CHECK(!rtype::read_score_list(bytes({0x15, 0x01, 0x01, 0x00, 0x41})).has_value());
    CHECK(!rtype::read_score_list(bytes({0x15, 0x00, 0x00})).has_value());
    // More than 10 scores is malformed, and never written.
    CHECK(!rtype::read_score_list(bytes({0x15, 0x0B})).has_value());
    const std::vector<ScoreRecord> many(rtype::max_listed_scores + 2,
                                        ScoreRecord{.name = "R9", .score = 1});
    const std::optional<std::vector<ScoreRecord>> written =
        rtype::read_score_list(rtype::write_score_list(many));
    REQUIRE(written.has_value());
    CHECK(written.value_or(std::vector<ScoreRecord>{}).size() == rtype::max_listed_scores);
}
