#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/config.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/ecs/waves.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <rtype/entities.hpp>
#include <rtype/server_game.hpp>
#include <rtype/tuning.hpp>
#include <rtype/waves.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using aegis2d::Vec2;
using aegis2d::core::ConfigError;
using aegis2d::core::step_from_rate;
using aegis2d::ecs::AutoTrigger;
using aegis2d::ecs::Entity;
using aegis2d::ecs::Registry;
using aegis2d::ecs::SineMotion;
using aegis2d::ecs::Transform;
using aegis2d::ecs::WaveSchedule;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;
using rtype::ServerGame;
namespace tuning = rtype::tuning;

namespace {

std::filesystem::path stage1() {
    return std::filesystem::path{RTYPE_ASSET_DIR} / "waves/stage1.json";
}

// Writes a waves file in a temporary folder, as a level designer would, and returns its path.
std::filesystem::path write_waves(std::string_view name, std::string_view text) {
    const auto folder = std::filesystem::temp_directory_path() / "rtype_waves_test";
    std::filesystem::create_directories(folder);
    const auto path = folder / name;
    std::ofstream{path} << text;
    return path;
}

// Tick and position of each enemy spawned by the waves during the first ticks of a game.
std::vector<std::pair<std::uint64_t, Vec2>> spawns_of(ServerGame& game, int ticks) {
    std::vector<std::pair<std::uint64_t, Vec2>> spawns;
    for (int tick = 0; tick < ticks; ++tick) {
        game.tick(step_from_rate(tuning::tick_rate));
        for (const Entity enemy : game.spawned()) {
            spawns.emplace_back(game.ticks(), game.registry().get<Transform>(enemy).position);
        }
    }
    return spawns;
}

} // namespace

TEST_CASE("the stage 1 waves are valid and stay in the Bydos lanes", "[rtype][waves][146]") {
    const WaveSchedule waves = rtype::load_waves(stage1());
    CHECK(waves.waves() >= 4);
    CHECK(waves.loops());
    CHECK(waves.spawns().size() >= 20);
    for (const auto& spawn : waves.spawns()) {
        INFO("wave " << spawn.wave << " at " << spawn.time << " s");
        CHECK(spawn.type == "bydos_slave");
        CHECK(spawn.position.x >= tuning::area_width);
        CHECK(spawn.position.y >= tuning::bydos_lane_top);
        CHECK(spawn.position.y <= tuning::bydos_lane_bottom);
    }
    CHECK(rtype::default_waves_file().filename() == "stage1.json");
}

TEST_CASE("a new wave needs no recompilation", "[rtype][waves][146]") {
    const auto file = write_waves("new_wave.json", R"({
        // Written at run time: three Bydos, one every second, 100 px apart.
        "waves": [{"delay": 0.5, "spawns": [
            {"type": "bydos_slave", "position": [1300, 200], "count": 3, "every": 1.0,
             "step": [0, 100]}
        ]}]
    })");
    ServerGame game;
    game.play_waves(rtype::load_waves(file));
    const auto spawns = spawns_of(game, 60 * 4);
    REQUIRE(spawns.size() == 3);
    for (std::size_t index = 0; index < spawns.size(); ++index) {
        const auto expected = static_cast<std::uint64_t>((0.5 + static_cast<double>(index)) * 60);
        CHECK(spawns.at(index).first + 1 >= expected);
        CHECK(spawns.at(index).first <= expected + 1);
        CHECK(spawns.at(index).second.y == 200.0F + (100.0F * static_cast<float>(index)));
    }
    CHECK(game.waves().finished());
    CHECK(spawns_of(game, 60 * 10).empty());
    std::filesystem::remove(file);
}

TEST_CASE("wave enemies are Bydos slaves, as dangerous as the others", "[rtype][waves][146]") {
    Registry registry;
    const Entity bydos = rtype::spawn_enemy(registry, "bydos_slave", Vec2{1296.0F, 300.0F});
    CHECK(registry.get<Transform>(bydos).position == Vec2{1296.0F, 300.0F});
    CHECK(registry.has<SineMotion>(bydos));
    CHECK(registry.has<AutoTrigger>(bydos));
    CHECK_THROWS_AS(rtype::spawn_enemy(registry, "boss", Vec2{}), std::invalid_argument);
    CHECK(rtype::enemy_types.size() == 1);
}

TEST_CASE("a typo in a waves file is reported with its place", "[rtype][waves][146]") {
    const auto file = write_waves("typo.json", R"({"waves": [
        {"spawns": [{"type": "bydos_slave", "position": [1300, 200]}]},
        {"spawns": [{"type": "bydos_slvae", "position": [1300, 200]}]}
    ]})");
    CHECK_THROWS_WITH(rtype::load_waves(file),
                      StartsWith(file.string()) &&
                          ContainsSubstring("waves.1.spawns.0.type: unknown type \"bydos_slvae\""));
    CHECK_THROWS_AS(rtype::load_waves(file.parent_path() / "missing.json"), ConfigError);
    std::filesystem::remove(file);
}

TEST_CASE("waves and random spawns add up, the same for a seed", "[rtype][waves][136][146]") {
    ServerGame first{7};
    ServerGame second{7};
    ServerGame waves_only;
    for (ServerGame* game : {&first, &second, &waves_only}) {
        game->play_waves(rtype::load_waves(stage1()));
    }
    const auto spawns = spawns_of(first, 60 * 30);
    CHECK(spawns == spawns_of(second, 60 * 30));
    const auto from_waves = spawns_of(waves_only, 60 * 30);
    CHECK(!from_waves.empty());
    CHECK(spawns.size() > from_waves.size());
}

TEST_CASE("without waves, a game spawns nothing on its own", "[rtype][waves][146]") {
    ServerGame game;
    CHECK(game.waves().finished());
    CHECK(game.waves().waves() == 0);
    CHECK(spawns_of(game, 60 * 10).empty());
    CHECK(game.registry().size() == 0);
}
