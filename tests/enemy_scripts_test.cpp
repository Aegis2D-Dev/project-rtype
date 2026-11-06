#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/config.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/shmup/components.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <rtype/enemy_scripts.hpp>
#include <rtype/server_game.hpp>
#include <rtype/tuning.hpp>
#include <rtype/waves.hpp>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using aegis2d::Vec2;
using aegis2d::core::ConfigError;
using aegis2d::core::step_from_rate;
using aegis2d::ecs::Entity;
using aegis2d::ecs::Registry;
using aegis2d::ecs::Transform;
using aegis2d::ecs::Velocity;
using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using rtype::EnemyScripts;
using rtype::ServerGame;
namespace tuning = rtype::tuning;

namespace {

using File = std::pair<std::string_view, std::string_view>;

// A folder of the test's own files, emptied first.
std::filesystem::path write_folder(std::string_view name, std::initializer_list<File> files) {
    const auto folder = std::filesystem::temp_directory_path() / "rtype_enemy_scripts_test" / name;
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    for (const auto& [file, text] : files) {
        std::ofstream{folder / file} << text;
    }
    return folder;
}

void run(ServerGame& game, int ticks) {
    for (int tick = 0; tick < ticks; ++tick) {
        game.tick(step_from_rate(tuning::tick_rate));
    }
}

// A new enemy, written by the test: it dives at 200 px/s, then climbs back after half a
// second, and never fires.
constexpr std::string_view diver = R"lua(
enemy {
    width = 24, height = 20, health = 2, score = 250,
    spawn = function(self)
        local velocity = ecs.get(self.entity, "Velocity")
        velocity.x, velocity.y = -200, 200
    end,
    update = function(self, dt)
        if self.age >= 0.5 then
            ecs.get(self.entity, "Velocity").y = -200
        end
    end,
}
)lua";

} // namespace

TEST_CASE("a new enemy type needs a script and a waves entry, no recompilation",
          "[rtype][enemies][356]") {
    const auto enemies = write_folder("new_type", {{"diver.lua", diver}});
    const auto waves = write_folder("new_type_waves", {{"dive.json", R"({"waves": [{"spawns": [
        {"type": "diver", "position": [1000, 200]}
    ]}]})"}}) / "dive.json";
    CHECK(rtype::enemy_types(enemies) == std::vector<std::string>{"diver"});

    ServerGame game{std::nullopt, enemies};
    game.play_waves(rtype::load_waves(waves, rtype::enemy_types(enemies)));
    run(game, 1);
    REQUIRE(game.spawned().size() == 1);
    const Entity enemy = game.spawned().front();
    const Registry& world = game.registry();
    CHECK(world.get<aegis2d::ecs::Sprite>(enemy).size == Vec2{24.0F, 20.0F});
    CHECK(world.get<aegis2d::ecs::Health>(enemy).points == 2);
    CHECK(world.get<aegis2d::ecs::ScoreValue>(enemy).points == 250);
    CHECK_FALSE(world.has<aegis2d::shmup::Weapon>(enemy));

    // It moves from the next tick on; its script turns it after half a second.
    const Vec2 start = world.get<Transform>(enemy).position;
    CHECK(start == Vec2{1000.0F, 200.0F});
    run(game, 31);
    CHECK(world.get<Velocity>(enemy).value == Vec2{-200.0F, -200.0F});
    run(game, 29);
    CHECK(world.get<Transform>(enemy).position.x == Approx(start.x - 200.0F).margin(0.1));
    // Back on its line, within the two ticks the sum of its ages may turn it late.
    CHECK(world.get<Transform>(enemy).position.y == Approx(start.y).margin(7.0));
}

TEST_CASE("the Bydos slave is a script with the values of its sprite", "[rtype][enemies][356]") {
    Registry registry;
    const EnemyScripts scripts{registry};
    const rtype::EnemyType* bydos = scripts.find(tuning::random_enemy);
    REQUIRE(bydos != nullptr);
    CHECK(bydos->size == tuning::bydos_size);
    CHECK(bydos->fire_every == 2.0F);
    CHECK(scripts.find("boss") == nullptr);
}

TEST_CASE("a type with no values gets the defaults", "[rtype][enemies][356]") {
    Registry registry;
    EnemyScripts scripts{registry, write_folder("defaults", {{"rock.lua", "enemy {}"}})};
    const Entity rock = scripts.spawn("rock", Vec2{600.0F, 300.0F});
    CHECK(scripts.types().front().size == Vec2{32.0F, 32.0F});
    CHECK(registry.get<Velocity>(rock).value == Vec2{});
    CHECK(registry.get<aegis2d::ecs::Health>(rock).points == 1);
    CHECK_FALSE(registry.has<aegis2d::shmup::Weapon>(rock));
    scripts.update(1.0F);
    CHECK(registry.get<Transform>(rock).position == Vec2{600.0F, 300.0F});
}

TEST_CASE("a wrong enemy script is refused with its file", "[rtype][enemies][356]") {
    Registry registry;
    const auto refused = [&registry](std::string_view name, File file) {
        return EnemyScripts{registry, write_folder(name, {file})};
    };
    CHECK_THROWS_WITH(refused("syntax", {"bad.lua", "enemy {\n  width = \n"}),
                      ContainsSubstring("bad.lua") && ContainsSubstring(":3:"));
    CHECK_THROWS_WITH(refused("silent", {"quiet.lua", "local x = 1"}),
                      ContainsSubstring("quiet.lua") &&
                          ContainsSubstring("does not call enemy { ... }"));
    CHECK_THROWS_WITH(refused("value", {"big.lua", R"(enemy { width = "big" })"}),
                      ContainsSubstring("width is not a number"));
    CHECK_THROWS_WITH(refused("zero", {"ghost.lua", "enemy { health = 0 }"}),
                      ContainsSubstring("health must be at least 1"));
    CHECK_THROWS_WITH(refused("twice", {"twin.lua", "enemy {} enemy {}"}),
                      ContainsSubstring("called once"));
    CHECK_THROWS_WITH(refused("behavior", {"odd.lua", "enemy { update = 3 }"}),
                      ContainsSubstring("update is not a function"));
    CHECK_THROWS_WITH(refused("name", {"Big-One.lua", "enemy {}"}),
                      ContainsSubstring("lowercase letters, digits and _"));
    CHECK_THROWS_AS(ServerGame(42, write_folder("no_random", {{"rock.lua", "enemy {}"}})),
                    ConfigError);
}

TEST_CASE("a failing behavior stops its type only, once", "[rtype][enemies][356]") {
    Registry registry;
    EnemyScripts scripts{registry, write_folder("failing", {
                                                               {"broken.lua", R"lua(
            enemy {
                spawn = function(self) ecs.get(self.entity, "Velocity").x = -50 end,
                update = function(self) failures = (failures or 0) + 1; error("boom") end,
            })lua"},
                                                               {"mover.lua", R"lua(
            enemy { update = function(self, dt) ecs.get(self.entity, "Transform").x = 10 * self.age end })lua"},
                                                           })};
    const Entity first = scripts.spawn("broken", Vec2{});
    const Entity second = scripts.spawn("broken", Vec2{});
    const Entity mover = scripts.spawn("mover", Vec2{});
    for (int tick = 0; tick < 3; ++tick) {
        scripts.update(1.0F);
    }
    CHECK(scripts.lua().number("failures") == 1.0);
    CHECK(registry.get<Velocity>(first).value.x == -50.0F);
    CHECK(registry.get<Velocity>(second).value.x == -50.0F);
    CHECK(registry.get<Transform>(mover).position.x == Approx(30.0F));
}

TEST_CASE("an enemy whose behavior is stopped keeps its speed", "[rtype][enemies][356]") {
    ServerGame game;
    const Entity bydos = game.spawn_enemy("bydos_slave", Vec2{1000.0F, 300.0F});
    game.enemies().stop(bydos);
    run(game, tuning::tick_rate / 2);
    CHECK(game.registry().get<Transform>(bydos).position.y == 300.0F);
    CHECK(game.registry().get<Transform>(bydos).position.x == Approx(940.0F).margin(0.01));
}

TEST_CASE("the scripts forget the enemies destroyed", "[rtype][enemies][356]") {
    Registry registry;
    EnemyScripts scripts{registry, write_folder("forget", {{"counted.lua", R"lua(
        enemy { update = function(self) seen = (seen or 0) + 1 end })lua"}})};
    const Entity gone = scripts.spawn("counted", Vec2{});
    scripts.spawn("counted", Vec2{});
    scripts.update(0.1F);
    registry.destroy(gone);
    scripts.update(0.1F);
    CHECK(scripts.lua().number("seen") == 3.0);
    CHECK_THROWS_AS(scripts.spawn("boss", Vec2{}), std::invalid_argument);
}
