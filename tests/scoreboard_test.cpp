#include <catch2/catch_test_macros.hpp>
#include <rtype/scoreboard.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using rtype::Scoreboard;
using rtype::ScoreEntry;

namespace {

// A file of a fresh temporary folder, for the board of one test.
std::filesystem::path board_file(std::string_view test) {
    const auto folder = std::filesystem::temp_directory_path() / "rtype_scoreboard_test" / test;
    std::filesystem::remove_all(folder);
    return folder / "scores.json";
}

void write(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream{file} << text;
}

} // namespace

TEST_CASE("the best scores survive a restart", "[server][scores]") {
    const std::filesystem::path file = board_file("restart");
    {
        Scoreboard board{file};
        CHECK(board.best().empty());
        CHECK(!board.load_error().has_value());
        CHECK(board.record("R9", 300));
        CHECK(board.record("Kaz", 1200));
        CHECK(board.record("Lolo", 300));
        CHECK(board.best() == std::vector<ScoreEntry>{{.name = "Kaz", .score = 1200},
                                                      {.name = "R9", .score = 300},
                                                      {.name = "Lolo", .score = 300}});
    }
    // Written whole, with no temporary file left.
    CHECK(std::filesystem::exists(file));
    CHECK(!std::filesystem::exists(file.string() + ".tmp"));
    const Scoreboard restarted{file};
    CHECK(!restarted.load_error().has_value());
    CHECK(restarted.best() == std::vector<ScoreEntry>{{.name = "Kaz", .score = 1200},
                                                      {.name = "R9", .score = 300},
                                                      {.name = "Lolo", .score = 300}});
}

TEST_CASE("the board keeps the 10 best scores", "[server][scores]") {
    const std::filesystem::path file = board_file("bounded");
    Scoreboard board{file};
    for (std::uint32_t score = 1; score <= Scoreboard::max_entries + 2; ++score) {
        CHECK(board.record("p" + std::to_string(score), score * 100));
    }
    REQUIRE(board.best().size() == Scoreboard::max_entries);
    CHECK(board.best().front() == ScoreEntry{.name = "p12", .score = 1200});
    CHECK(board.best().back() == ScoreEntry{.name = "p3", .score = 300});
    // Not among the best, or not a name: not recorded.
    CHECK(!board.record("low", 200));
    CHECK(!board.record("", 5000));
    CHECK(!board.record("a\nb", 5000));
    CHECK(Scoreboard{file}.best() == board.best());

    // A hand-edited file with more scores, out of order, keeps the 10 best, best first.
    std::string text = R"({"scores": [)";
    for (std::size_t index = 0; index < Scoreboard::max_entries + 3; ++index) {
        text += (index == 0 ? "" : ",") + std::string{R"({"name": "p", "score": )"} +
                std::to_string(index) + "}";
    }
    write(file, text + "]}");
    const Scoreboard edited{file};
    REQUIRE(edited.best().size() == Scoreboard::max_entries);
    CHECK(edited.best().front().score == Scoreboard::max_entries + 2);
    CHECK(edited.best().back().score == 3);
}

TEST_CASE("a corrupted board is reported, not fatal", "[server][scores]") {
    const std::filesystem::path file = board_file("corrupted");
    for (const std::string_view text :
         {std::string_view{"{ not json"}, std::string_view{R"({"scores": 3})"},
          std::string_view{R"({"scores": [{"name": "R9", "score": "many"}]})"},
          std::string_view{R"({"scores": [{"name": "", "score": 10}]})"},
          std::string_view{R"({"scores": [{"name": "R9", "score": -1}]})"}}) {
        CAPTURE(text);
        write(file, text);
        Scoreboard board{file};
        CHECK(board.load_error().has_value());
        CHECK(board.best().empty());
        // The next score replaces the file with a valid board.
        CHECK(board.record("R9", 700));
        const Scoreboard reread{file};
        CHECK(!reread.load_error().has_value());
        CHECK(reread.best() == std::vector<ScoreEntry>{{.name = "R9", .score = 700}});
    }
}

TEST_CASE("a board that cannot be written stays in memory", "[server][scores]") {
    const std::filesystem::path folder = board_file("unwritable").parent_path();
    // The board's folder is a file: it cannot be created.
    write(folder.parent_path() / "blocked", "");
    Scoreboard board{folder.parent_path() / "blocked" / "scores.json"};
    CHECK(board.record("R9", 100));
    CHECK(board.best().size() == 1);
}
