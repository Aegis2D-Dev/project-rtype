#include <aegis2d/core/config.hpp>
#include <aegis2d/core/paths.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/script/components.hpp>
#include <aegis2d/script/events.hpp>
#include <aegis2d/shmup/components.hpp>

#include <rtype/enemy_scripts.hpp>
#include <rtype/layers.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace rtype {

namespace core = aegis2d::core;
namespace ecs = aegis2d::ecs;
namespace script = aegis2d::script;
namespace shmup = aegis2d::shmup;
using aegis2d::Vec2;

namespace {

// The game's side of the scripts, run once before them: `enemy { ... }` keeps the type of the
// file being loaded, the EnemySpawned listener gives each enemy its table and runs `spawn`,
// and _update_enemies runs `update`. A behavior that fails is logged and stopped (not at
// every tick), and the other enemies go on; one that takes the whole budget is cut off with
// the run, which pcall cannot catch, so the C++ side stops it with _stop_running(). A spawn
// that failed leaves enemy_spawn_failed set, for the C++ side to destroy the enemy.
constexpr std::string_view prelude = R"lua(
local types, order, enemies, broken, loading, running = {}, {}, {}, {}, nil, nil

function enemy(definition)
    if type(definition) ~= "table" then
        error("enemy takes a table: enemy { ... }", 2)
    end
    if loading == nil or types[loading] ~= nil then
        error("enemy { ... } is called once, by the file of the type", 2)
    end
    types[loading] = definition
end

function _load_enemy(name)
    loading = name
    order[#order + 1] = name
end

function _describe_enemy()
    local definition = types[loading]
    if definition == nil then
        error("the file does not call enemy { ... }", 0)
    end
    for _, field in ipairs({ "width", "height", "health", "score", "fire_every", "first_shot" }) do
        local value = rawget(definition, field)
        if value ~= nil and type(value) ~= "number" then
            error(field .. " is not a number", 0)
        end
        _G["enemy_" .. field] = value
    end
    for _, field in ipairs({ "spawn", "update" }) do
        local value = rawget(definition, field)
        if value ~= nil and type(value) ~= "function" then
            error(field .. " is not a function", 0)
        end
    end
    loading = nil
end

local function stop(behavior, name, why)
    broken[behavior] = true
    print("enemy " .. name .. " stopped: " .. tostring(why))
end

-- Runs a behavior; false if it failed, now or before.
local function behave(self, behavior, ...)
    if behavior == nil then
        return true
    end
    if broken[behavior] then
        return false
    end
    running = { behavior = behavior, type = self.type }
    local ok, message = pcall(behavior, self, ...)
    running = nil
    if not ok then
        stop(behavior, self.type, message)
    end
    return ok
end

function _stop_running()
    if running ~= nil then
        stop(running.behavior, running.type, "too many instructions or too much memory")
        running = nil
    end
end

events.on("EnemySpawned", function(event)
    local name = order[event.type]
    local self = { entity = event.entity, type = name, x = event.x, y = event.y,
                   random = event.random, age = 0 }
    enemy_spawn_failed = true
    if behave(self, types[name].spawn) then
        enemies[event.entity] = self
        enemy_spawn_failed = false
    end
end)

function _stop_enemy(entity)
    enemies[entity] = nil
end

function _update_enemies(dt)
    for entity, self in pairs(enemies) do
        if ecs.alive(entity) then
            self.age = self.age + dt
            behave(self, types[self.type].update, dt)
        else
            enemies[entity] = nil
        end
    end
end
)lua";

// Largest width or height of an enemy, far beyond the screen.
constexpr float max_size = 65536.0F;

bool type_name(std::string_view name) noexcept {
    return !name.empty() && std::ranges::all_of(name, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

// The error of a script, named by its file: Lua names it already for an error in the file.
[[noreturn]] void fail(const std::filesystem::path& file, const script::ScriptState& lua) {
    const std::optional<script::ScriptError>& error = lua.last_error();
    // run_file() names the chunk with the generic path, `/` on Windows too.
    if (error && error->chunk == file.generic_string()) {
        throw core::ConfigError{script::to_string(*error)};
    }
    throw core::ConfigError{std::format("{}: {}", file.generic_string(),
                                        error ? error->message : std::string{"failed"})};
}

// The values `enemy { ... }` gave, which _describe_enemy() left in globals.
EnemyType describe(std::string name, const std::filesystem::path& file,
                   const script::ScriptState& lua) {
    EnemyType type{.name = std::move(name)};
    const auto value = [&lua](std::string_view field, float fallback) {
        return static_cast<float>(lua.number(std::format("enemy_{}", field)).value_or(fallback));
    };
    type.size = Vec2{value("width", type.size.x), value("height", type.size.y)};
    const float health = value("health", static_cast<float>(type.health));
    const float score = value("score", static_cast<float>(type.score));
    type.fire_every = value("fire_every", type.fire_every);
    type.first_shot = value("first_shot", type.first_shot);
    const auto wrong = [&file](std::string_view what) {
        return core::ConfigError{std::format("{}: {}", file.generic_string(), what)};
    };
    // Negated, so that NaN fails too; the bounds of the integers are powers of 2, exact floats.
    if (!(type.size.x > 0.0F && type.size.x < max_size) ||
        !(type.size.y > 0.0F && type.size.y < max_size)) {
        throw wrong("width and height must be above 0 and below 65536");
    }
    if (!(health >= 1.0F && health < 2147483648.0F) || !(score >= 0.0F && score < 4294967296.0F)) {
        throw wrong("health must be from 1 to 2^31 - 1 and score from 0 to 2^32 - 1");
    }
    if (!(type.fire_every >= 0.0F && std::isfinite(type.fire_every)) ||
        !(type.first_shot >= 0.0F && std::isfinite(type.first_shot))) {
        throw wrong("fire_every and first_shot must be at least 0, and finite");
    }
    type.health = static_cast<std::int32_t>(health);
    type.score = static_cast<std::uint32_t>(score);
    return type;
}

} // namespace

std::filesystem::path default_enemies_directory() {
    return core::asset_path("enemies");
}

std::vector<std::string> enemy_types(const std::filesystem::path& directory) {
    std::vector<std::string> names;
    std::error_code error;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator{directory, error}) {
        if (entry.path().extension() == ".lua") {
            names.push_back(entry.path().stem().string());
        }
    }
    std::ranges::sort(names);
    return names;
}

EnemyScripts::EnemyScripts(ecs::Registry& registry, const std::filesystem::path& directory)
    : registry_{&registry} {
    script::bind_components(lua_, registry);
    if (!lua_.run(prelude, "enemy_scripts")) {
        fail("enemy_scripts", lua_);
    }
    for (std::string& name : enemy_types(directory)) {
        const std::filesystem::path file = directory / (name + ".lua");
        if (!type_name(name)) {
            throw core::ConfigError{
                std::format("{}: an enemy type is named with lowercase letters, digits and _",
                            file.generic_string())};
        }
        if (!lua_.run(std::format("_load_enemy('{}')", name), "enemy_scripts") ||
            !lua_.run_file(file) || !lua_.call("_describe_enemy")) {
            fail(file, lua_);
        }
        types_.push_back(describe(std::move(name), file, lua_));
    }
}

const std::vector<EnemyType>& EnemyScripts::types() const noexcept {
    return types_;
}

const EnemyType* EnemyScripts::find(std::string_view name) const noexcept {
    const auto found = std::ranges::find(types_, name, &EnemyType::name);
    return found == types_.end() ? nullptr : &*found;
}

ecs::Entity EnemyScripts::spawn(std::string_view type, Vec2 position, float random,
                                float shot_delay) {
    const auto found = std::ranges::find(types_, type, &EnemyType::name);
    if (found == types_.end()) {
        throw std::invalid_argument{std::format("rtype: unknown enemy type \"{}\"", type)};
    }
    ecs::Registry& world = *registry_;
    const ecs::Entity enemy = world.create();
    world.emplace<ecs::Transform>(enemy, position);
    world.emplace<ecs::Velocity>(enemy, Vec2{});
    world.emplace<ecs::Sprite>(enemy, ecs::Sprite{.size = found->size});
    world.emplace<ecs::Collider>(
        enemy, ecs::Collider{.size = found->size, .layer = layer::enemy, .mask = mask::enemy});
    world.emplace<ecs::Health>(enemy, found->health);
    world.emplace<ecs::Damage>(enemy,
                               ecs::Damage{.amount = tuning::ram_damage, .consumed_on_hit = false});
    world.emplace<ecs::ScoreValue>(enemy, found->score);
    if (found->fire_every > 0.0F) {
        world.emplace<shmup::Weapon>(
            enemy,
            shmup::Weapon{.cooldown = found->fire_every, .reload = found->first_shot + shot_delay});
        world.emplace<shmup::AutoTrigger>(enemy);
    }
    world.emplace<ecs::OffscreenDespawn>(enemy, tuning::despawn_margin);
    world.emplace<ecs::NetworkId>(enemy, ecs::NetworkId::of(enemy));
    const auto index = static_cast<std::size_t>(found - types_.begin()) + 1;
    const std::array fields{
        script::EventField{.name = "entity", .value = static_cast<double>(enemy.id())},
        script::EventField{.name = "type", .value = static_cast<double>(index)},
        script::EventField{.name = "x", .value = position.x},
        script::EventField{.name = "y", .value = position.y},
        script::EventField{.name = "random", .value = random},
    };
    if (!script::emit(lua_, "EnemySpawned", fields)) {
        lua_.call("_stop_running");
    }
    if (lua_.boolean("enemy_spawn_failed").value_or(true)) {
        world.destroy(enemy);
        return ecs::Entity{};
    }
    return enemy;
}

void EnemyScripts::stop(ecs::Entity enemy) {
    lua_.run(std::format("_stop_enemy({})", enemy.id()), "enemy_scripts");
}

void EnemyScripts::update(float seconds) {
    if (!lua_.call("_update_enemies", seconds)) {
        lua_.call("_stop_running");
    }
}

script::ScriptState& EnemyScripts::lua() noexcept {
    return lua_;
}

} // namespace rtype
