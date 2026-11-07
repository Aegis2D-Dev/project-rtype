#include <aegis2d/core/config.hpp>
#include <aegis2d/ecs/despawn.hpp>
#include <aegis2d/ecs/movement.hpp>
#include <aegis2d/shmup/weapons.hpp>

#include <rtype/entities.hpp>
#include <rtype/lag_compensation.hpp>
#include <rtype/layers.hpp>
#include <rtype/server_game.hpp>
#include <rtype/ship_movement.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace rtype {

namespace ecs = aegis2d::ecs;
namespace shmup = aegis2d::shmup;
using aegis2d::Vec2;

namespace {

ecs::SpawnSettings bydos_spawns(float scale) noexcept {
    return ecs::SpawnSettings{.from = Vec2{tuning::bydos_spawn_x, tuning::bydos_lane_top},
                              .to = Vec2{tuning::bydos_spawn_x, tuning::bydos_lane_bottom},
                              .min_interval = tuning::bydos_spawn_min_interval * scale,
                              .max_interval = tuning::bydos_spawn_max_interval * scale};
}

// The health an enemy's script gave it, times the factor of the room's rules.
void scale_health(ecs::Registry& world, ecs::Entity enemy, std::int32_t factor) {
    if (const auto health = world.try_get<ecs::Health>(enemy)) {
        health->get().points *= factor;
    }
}

bool on_screen(const ecs::Registry& world, ecs::Entity entity) {
    const Vec2 center = world.get<ecs::Transform>(entity).position;
    const ecs::Aabb area = ServerGame::play_area();
    return center.x >= area.min.x && center.x <= area.max.x && center.y >= area.min.y &&
           center.y <= area.max.y;
}

} // namespace

ServerGame::ServerGame(std::optional<std::uint64_t> seed, const std::filesystem::path& enemies,
                       bool hot_reload, GameRules rules)
    : enemies_{registry_, enemies}, hot_{hot_reload},
      grid_{Vec2{tuning::area_width, tuning::area_height}, tuning::collision_cell}, seed_{seed},
      rules_{rules}, spawner_{bydos_spawns(rules.spawn_interval_scale), seed.value_or(0)} {
    if (seed_ && enemies_.find(tuning::random_enemy) == nullptr) {
        throw aegis2d::core::ConfigError{std::format("{}: no script gives {}, the random enemy",
                                                     enemies.string(), tuning::random_enemy)};
    }
    register_systems();
    systems_.set_enabled("spawn", seed_.has_value());
    enemies_.watch(hot_);
}

void ServerGame::register_systems() {
    // A player ship moves as step_ship() says: steering, movement, then play_area.
    systems_.add("steering", steer_ships);
    systems_.add("movement", ecs::apply_velocity);
    systems_.add("behaviors", [this](ecs::Registry& /*world*/, const ecs::Tick& tick) {
        enemies_.update(tick.delta());
    });
    systems_.add("play_area", keep_ships_in_area);
    systems_.add("weapons", [this](ecs::Registry& world, const ecs::Tick& tick) {
        shmup::trigger_weapons(world, tick, shooters_);
        fired_.clear();
        for (const ecs::Entity shooter : shooters_) {
            const ecs::Entity missile = spawn_player_missile(world, shooter);
            if (rules_.friendly_fire) {
                world.get<ecs::Collider>(missile).mask |= layer::player;
            }
            fired_.push_back(missile);
        }
    });
    systems_.add("enemy_fire", [this](ecs::Registry& world, const ecs::Tick& tick) {
        shmup::trigger_auto_weapons(world, tick, gunners_);
        enemy_fired_.clear();
        for (const ecs::Entity gunner : gunners_) {
            if (on_screen(world, gunner)) {
                enemy_fired_.push_back(fire_enemy_missile(world, gunner));
            }
        }
    });
    systems_.add("spawn", [this](ecs::Registry& /*world*/, const ecs::Tick& tick) {
        spawner_.update(tick, spawn_points_);
        const float fire_every = enemies_.find(tuning::random_enemy)->fire_every;
        for (const Vec2 line : spawn_points_) {
            const float random = spawner_.random().uniform(0.0F, 1.0F);
            const float shot_delay = spawner_.random().uniform(0.0F, fire_every);
            const ecs::Entity enemy =
                enemies_.spawn(tuning::random_enemy, line, random, shot_delay);
            if (registry_.alive(enemy)) {
                scale_health(registry_, enemy, rules_.enemy_health_factor);
                spawned_.push_back(enemy);
            }
        }
    });
    systems_.add("waves", [this](ecs::Registry& /*world*/, const ecs::Tick& tick) {
        waves_.update(tick, due_);
        for (const ecs::WaveSpawn& spawn : due_) {
            const ecs::Entity enemy = enemies_.spawn(spawn.type, spawn.position);
            if (registry_.alive(enemy)) {
                scale_health(registry_, enemy, rules_.enemy_health_factor);
                spawned_.push_back(enemy);
            }
        }
    });
    systems_.add("collision", [this](ecs::Registry& world, const ecs::Tick& tick) {
        grid_.build(world);
        ecs::find_contacts(world, grid_.pairs(), contacts_);
        // Lag compensation: each player's missiles also meet the enemies where that player
        // saw them; the state of this tick will be tick.index + 1 in the history.
        find_rewound_contacts(
            world, history_, tick.index + 1,
            [this](ecs::Entity shooter) { return rewind(shooter); }, contacts_);
    });
    systems_.add("damage", [this](ecs::Registry& world, const ecs::Tick& /*tick*/) {
        ecs::apply_damage(world, contacts_, damage_);
    });
    systems_.add("score", [this](ecs::Registry& world, const ecs::Tick& /*tick*/) {
        ecs::credit_kills(world, damage_, awards_);
    });
    systems_.add("lives", [this](ecs::Registry& world, const ecs::Tick& /*tick*/) {
        ecs::handle_player_deaths(world, damage_, lives_lost_);
    });
    systems_.add("cleanup", [this](ecs::Registry& world, const ecs::Tick& /*tick*/) {
        ecs::remove_casualties(world, damage_);
    });
    systems_.add("despawn", [this](ecs::Registry& world, const ecs::Tick& /*tick*/) {
        ecs::despawn_outside(world, play_area(), despawned_);
    });
}

ecs::Entity ServerGame::add_player() {
    const auto free =
        std::ranges::find_if(slots_, [this](ecs::Entity ship) { return !registry_.alive(ship); });
    if (free == slots_.end()) {
        throw std::length_error{"rtype: the game already has four players"};
    }
    return add_player(static_cast<std::size_t>(std::distance(slots_.begin(), free)));
}

ecs::Entity ServerGame::add_player(std::size_t slot) {
    if (slot >= slots_.size()) {
        throw std::out_of_range{"rtype: player slots go from 0 to 3"};
    }
    ecs::Entity& ship = slots_.at(slot);
    if (registry_.alive(ship)) {
        throw std::logic_error{"rtype: this player slot is taken"};
    }
    ship = spawn_player(registry_, slot);
    registry_.get<ecs::Lives>(ship).count = rules_.lives;
    rewinds_.at(slot) = 0;
    started_ = true;
    return ship;
}

bool ServerGame::remove_player(ecs::Entity player) {
    const std::optional<std::size_t> slot = slot_of(player);
    if (!slot) {
        return false;
    }
    registry_.destroy(player);
    slots_.at(*slot) = ecs::Entity{};
    return true;
}

std::optional<std::size_t> ServerGame::slot_of(ecs::Entity player) const noexcept {
    if (!registry_.alive(player)) {
        return std::nullopt;
    }
    const auto found = std::ranges::find(slots_, player);
    if (found == slots_.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(slots_.begin(), found));
}

std::size_t ServerGame::players() const {
    return ecs::players_left(registry_);
}

bool ServerGame::set_input(ecs::Entity player, ecs::PlayerInput input) {
    if (!slot_of(player)) {
        return false;
    }
    registry_.emplace_or_replace<ecs::PlayerInput>(player, input);
    return true;
}

void ServerGame::tick(aegis2d::core::Duration step) {
    hot_.update();
    spawned_.clear();
    systems_.update(registry_, step);
    history_.record(ticks(), registry_);
}

ecs::Entity ServerGame::spawn_enemy(std::string_view type, Vec2 position) {
    return enemies_.spawn(type, position);
}

EnemyScripts& ServerGame::enemies() noexcept {
    return enemies_;
}

void ServerGame::play_waves(ecs::WaveSchedule waves) {
    waves_ = std::move(waves);
}

const ecs::WaveSchedule& ServerGame::waves() const noexcept {
    return waves_;
}

bool ServerGame::game_over() const {
    return started_ && ecs::game_over(registry_);
}

std::uint64_t ServerGame::ticks() const noexcept {
    return systems_.ticks();
}

bool ServerGame::set_rewind(ecs::Entity player, std::uint32_t ticks) {
    const std::optional<std::size_t> slot = slot_of(player);
    if (!slot.has_value()) {
        return false;
    }
    rewinds_.at(*slot) = std::min<std::uint32_t>(ticks, PositionHistory::capacity);
    return true;
}

std::uint32_t ServerGame::rewind(ecs::Entity player) const {
    const std::optional<std::size_t> slot = slot_of(player);
    return slot.has_value() ? rewinds_.at(*slot) : 0;
}

const PositionHistory& ServerGame::history() const noexcept {
    return history_;
}

ecs::Registry& ServerGame::registry() noexcept {
    return registry_;
}

const ecs::Registry& ServerGame::registry() const noexcept {
    return registry_;
}

void ServerGame::set_profiler(aegis2d::core::Profiler* profiler) noexcept {
    systems_.set_profiler(profiler);
}

const ecs::Scheduler& ServerGame::systems() const noexcept {
    return systems_;
}

const std::vector<ecs::Entity>& ServerGame::fired() const noexcept {
    return fired_;
}

const std::vector<ecs::Entity>& ServerGame::enemy_fired() const noexcept {
    return enemy_fired_;
}

const std::vector<ecs::Entity>& ServerGame::spawned() const noexcept {
    return spawned_;
}

const GameRules& ServerGame::rules() const noexcept {
    return rules_;
}

std::optional<std::uint64_t> ServerGame::seed() const noexcept {
    return seed_;
}

const ecs::DamageReport& ServerGame::damage() const noexcept {
    return damage_;
}

const std::vector<ecs::Award>& ServerGame::awards() const noexcept {
    return awards_;
}

const std::vector<ecs::LifeLost>& ServerGame::lives_lost() const noexcept {
    return lives_lost_;
}

const std::vector<ecs::Entity>& ServerGame::despawned() const noexcept {
    return despawned_;
}

ecs::Aabb ServerGame::play_area() noexcept {
    return ecs::Aabb{.min = Vec2{0.0F, 0.0F}, .max = Vec2{tuning::area_width, tuning::area_height}};
}

} // namespace rtype
