#include <aegis2d/core/config.hpp>
#include <aegis2d/core/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <rtype/level_editor.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <optional>
#include <string_view>
#include <vector>

using aegis2d::Vec2;
using Catch::Approx;
using rtype::DraftEnemy;
using rtype::EditorPointer;
using rtype::EditorView;
using rtype::LevelDraft;
using rtype::LevelEditor;
namespace tuning = rtype::tuning;

namespace {

std::filesystem::path write_waves(std::string_view name, std::string_view text) {
    const auto folder = std::filesystem::temp_directory_path() / "rtype_level_editor_test";
    std::filesystem::create_directories(folder);
    const auto path = folder / name;
    std::ofstream{path} << text;
    return path;
}

// A draft of one wave with two enemies, far apart.
LevelDraft two_enemies() {
    return LevelDraft{
        .loop = false,
        .waves = {{.delay = 1.0F,
                   .enemies = {{.type = "bydos_slave", .position = {300.0F, 200.0F}},
                               {.type = "bydos_slave", .position = {900.0F, 500.0F}}}}}};
}

// The mouse over a point of the play area, as the editor's view draws it.
EditorPointer over(Vec2 level, bool pressed = false, bool held = false, bool remove = false) {
    return {.position = EditorView{}.to_screen(level),
            .pressed = pressed,
            .held = held,
            .remove = remove};
}

} // namespace

TEST_CASE("a waves file opens as one enemy per member of its groups", "[client][editor][369]") {
    const LevelDraft stage =
        LevelDraft::from_file(std::filesystem::path{RTYPE_ASSET_DIR} / "waves" / "stage1.json");
    CHECK(stage.loop);
    REQUIRE(stage.waves.size() == 4);
    const std::size_t enemies = std::accumulate(
        stage.waves.begin(), stage.waves.end(), std::size_t{0},
        [](std::size_t sum, const rtype::DraftWave& wave) { return sum + wave.enemies.size(); });
    CHECK(enemies == 29); // as r-type_server counts them
    CHECK(stage.waves.at(0).delay == 2.0F);
    REQUIRE(stage.waves.at(0).enemies.size() == 5);
    CHECK(stage.waves.at(0).enemies.at(4) ==
          DraftEnemy{
              .type = "bydos_slave", .position = {1296.0F, 136.0F + (4 * 112.0F)}, .at = 0.0F});
    // A file of six, one every half second, the second file two seconds in.
    const auto& files = stage.waves.at(1).enemies;
    REQUIRE(files.size() == 12);
    CHECK(files.at(5).at == Approx(2.5F));
    CHECK(files.at(6).position == Vec2{1296.0F, 520.0F});
    CHECK(files.at(6).at == Approx(2.0F));

    const auto broken = write_waves(
        "broken.json", R"({"waves": [{"spawns": [{"type": "boss", "position": [1, 2]}]}]})");
    CHECK_THROWS_AS(LevelDraft::from_file(broken), aegis2d::core::ConfigError);
    CHECK_THROWS_AS(LevelDraft::from_file(broken.parent_path() / "missing.json"),
                    aegis2d::core::ConfigError);
}

TEST_CASE("the editor's view shows the play area and the strip beyond it",
          "[client][editor][369]") {
    const EditorView view;
    const Vec2 right =
        view.to_screen({tuning::area_width + EditorView::strip, tuning::area_height});
    CHECK(right.x <= tuning::area_width);
    CHECK(right.y <= tuning::area_height);
    CHECK(view.to_screen({0.0F, 0.0F}).x >= 0.0F);
    const Vec2 point{1296.0F, 360.0F};
    CHECK(view.to_level(view.to_screen(point)).x == Approx(point.x));
    CHECK(view.to_level(view.to_screen(point)).y == Approx(point.y));
}

TEST_CASE("enemies are placed with the mouse", "[client][editor][369]") {
    LevelEditor editor{two_enemies()};
    CHECK_FALSE(editor.changed());
    editor.update(over({640.0F, 360.0F}, true, true));
    REQUIRE(editor.enemies().size() == 3);
    CHECK(editor.enemies().back() ==
          DraftEnemy{.type = "bydos_slave", .position = {640.0F, 360.0F}});
    CHECK(editor.dragged() == 2);
    CHECK(editor.changed());
    // Held, the new enemy follows the mouse; released, it stays.
    editor.update(over({700.4F, 380.6F}, false, true));
    CHECK(editor.enemies().back().position == Vec2{700.0F, 381.0F});
    editor.update(over({710.0F, 390.0F}));
    CHECK_FALSE(editor.dragged().has_value());
    CHECK(editor.enemies().back().position == Vec2{700.0F, 381.0F});
    CHECK(editor.hovered() == 2);

    // Beyond the strip or the bottom, it stops at their edge.
    editor.update(over({2000.0F, 900.0F}, true, true));
    CHECK(editor.enemies().back().position ==
          Vec2{tuning::area_width + EditorView::strip, tuning::area_height});
}

TEST_CASE("an enemy is moved and removed with the mouse", "[client][editor][369]") {
    LevelEditor editor{two_enemies()};
    // Taken a little off its center, it keeps that offset.
    editor.update(over({310.0F, 205.0F}, true, true));
    CHECK(editor.dragged() == 0);
    editor.update(over({410.0F, 255.0F}, false, true));
    editor.update(over({410.0F, 255.0F}));
    CHECK(editor.enemies().size() == 2);
    CHECK(editor.enemies().front().position == Vec2{400.0F, 250.0F});

    editor.update(over({900.0F, 500.0F}, false, false, true));
    REQUIRE(editor.enemies().size() == 1);
    CHECK(editor.enemies().front().position == Vec2{400.0F, 250.0F});
    // Nothing under the mouse: nothing removed.
    editor.update(over({100.0F, 100.0F}, false, false, true));
    CHECK(editor.enemies().size() == 1);
    CHECK_FALSE(editor.hovered().has_value());
}

TEST_CASE("the editor goes from wave to wave", "[client][editor][369]") {
    LevelDraft draft = two_enemies();
    draft.waves.push_back({.delay = 3.0F, .enemies = {}});
    LevelEditor editor{draft};
    CHECK(editor.wave() == 0);
    editor.next_wave();
    CHECK(editor.wave() == 1);
    CHECK(editor.enemies().empty());
    editor.update(over({640.0F, 360.0F}, true, true));
    CHECK(editor.draft().waves.at(1).enemies.size() == 1);
    CHECK(editor.draft().waves.at(0).enemies.size() == 2);
    editor.next_wave();
    CHECK(editor.wave() == 0);
    CHECK_FALSE(editor.dragged().has_value());
    editor.previous_wave();
    CHECK(editor.wave() == 1);

    LevelEditor empty{LevelDraft{}};
    CHECK(empty.draft().waves.size() == 1);
    empty.previous_wave();
    CHECK(empty.wave() == 0);
}
