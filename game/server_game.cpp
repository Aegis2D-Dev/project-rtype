#include <aegis2d/ecs/despawn.hpp>
#include <aegis2d/ecs/movement.hpp>
#include <aegis2d/ecs/weapons.hpp>

#include <rtype/entities.hpp>
#include <rtype/server_game.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stdexcept>

namespace rtype {

namespace ecs = aegis2d::ecs;
using aegis2d::Vec2;

namespace {

ecs::SpawnSettings bydos_spawns() noexcept {
    return ecs::SpawnSettings{.from = Vec2{tuning::bydos_spawn_x, tuning::bydos_lane_top},
                              .to = Vec2{tuning::bydos_spawn_x, tuning::bydos_lane_bottom},
                              .min_interval = tuning::bydos_spawn_min_interval,
                              .max_interval = tuning::bydos_spawn_max_interval};
}

} // namespace

ServerGame::ServerGame(std::optional<std::uint64_t> seed)
    : grid_{Vec2{tuning::area_width, tuning::area_height}, tuning::collision_cell}, seed_{seed},
      spawner_{bydos_spawns(), seed.value_or(0)} {
    register_systems();
    systems_.set_enabled("spawn", seed_.has_value());
}

void ServerGame::register_systems() {
    systems_.add("steering", ecs::steer_player_ships);
    systems_.add("movement", ecs::apply_velocity);
    systems_.add("sine_motion", ecs::apply_sine_motion);
    systems_.add("play_area", [](ecs::Registry& world, const ecs::Tick& /*tick*/) {
        ecs::clamp_player_ships(world, play_area());
    });
    systems_.add("weapons", [this](ecs::Registry& world, const ecs::Tick& tick) {
        ecs::trigger_weapons(world, tick, shooters_);
        fired_.clear();
        for (const ecs::Entity shooter : shooters_) {
            fired_.push_back(spawn_player_missile(world, shooter));
        }
    });
    systems_.add("spawn", [this](ecs::Registry& world, const ecs::Tick& tick) {
        spawner_.update(tick, spawn_points_);
        spawned_.clear();
        for (const Vec2 line : spawn_points_) {
            const float phase = spawner_.random().uniform(0.0F, tuning::bydos_wave_period);
            spawned_.push_back(spawn_bydos_slave(world, line, phase));
        }
    });
    systems_.add("collision", [this](ecs::Registry& world, const ecs::Tick& /*tick*/) {
        grid_.build(world);
        ecs::find_contacts(world, grid_.pairs(), contacts_);
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
    const auto slot = static_cast<std::size_t>(std::distance(slots_.begin(), free));
    *free = spawn_player(registry_, slot);
    started_ = true;
    return *free;
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
    systems_.update(registry_, step);
}

bool ServerGame::game_over() const {
    return started_ && ecs::game_over(registry_);
}

std::uint64_t ServerGame::ticks() const noexcept {
    return systems_.ticks();
}

ecs::Registry& ServerGame::registry() noexcept {
    return registry_;
}

const ecs::Registry& ServerGame::registry() const noexcept {
    return registry_;
}

const ecs::Scheduler& ServerGame::systems() const noexcept {
    return systems_;
}

const std::vector<ecs::Entity>& ServerGame::fired() const noexcept {
    return fired_;
}

const std::vector<ecs::Entity>& ServerGame::spawned() const noexcept {
    return spawned_;
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
