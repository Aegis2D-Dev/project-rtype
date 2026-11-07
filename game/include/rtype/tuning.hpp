/// @file
/// @brief Gameplay values of R-Type: play area, speeds, sizes, health, lives and scores.
#pragma once
#include <aegis2d/core/math.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

/// @brief Values tuned for the game, in logical pixels, seconds and health points.
namespace rtype::tuning {

/// @brief Simulation rate of the server, in ticks per second.
inline constexpr std::uint32_t tick_rate = 60;

/// @brief Snapshots the server sends each client per second (RFC section 4.3).
inline constexpr std::uint32_t snapshot_rate = 20;
static_assert(tick_rate % snapshot_rate == 0, "a snapshot every whole number of ticks");

/// @brief Ticks of enemy positions the server keeps, for lag compensation: 500 ms.
inline constexpr std::uint32_t position_history_ticks = tick_rate / 2;
/// @brief How far behind the server a client draws the world, as the server assumes it for
/// lag compensation: the client's first interpolation delay, which clients do not report.
inline constexpr std::chrono::milliseconds client_interpolation_delay{100};

/// @brief Width of the play area (logical resolution).
inline constexpr float area_width = 1280.0F;
/// @brief Height of the play area (logical resolution).
inline constexpr float area_height = 720.0F;

/// @brief Players in one game, as the subject requires.
inline constexpr std::size_t max_players = 4;

/// @brief Speed of a player ship, the same in every direction.
inline constexpr float player_speed = 300.0F;
/// @brief Size of a player ship on screen.
inline constexpr aegis2d::Vec2 player_size{33.0F, 17.0F};
/// @brief Hit box of a player ship, a little smaller than its sprite.
inline constexpr aegis2d::Vec2 player_hitbox{27.0F, 11.0F};
/// @brief Health points of a player ship: one hit loses a life.
inline constexpr std::int32_t player_health = 1;
/// @brief Lives of a player at the start of a game.
inline constexpr std::uint32_t player_lives = 3;
/// @brief Distance of the player spawns from the left edge.
inline constexpr float player_spawn_x = 100.0F;

/// @brief Seconds between two shots of a player holding fire (four shots a second).
inline constexpr float player_fire_cooldown = 0.25F;

/// @brief Speed of a player missile, to the right.
inline constexpr float player_missile_speed = 900.0F;
/// @brief Size of a player missile.
inline constexpr aegis2d::Vec2 player_missile_size{16.0F, 4.0F};
/// @brief Damage of a player missile.
inline constexpr std::int32_t player_missile_damage = 1;

/// @brief Size of a Bydos slave, as the clients draw it: `width` and `height` of
/// `assets/enemies/bydos_slave.lua`, where its other values and its behavior are.
inline constexpr aegis2d::Vec2 bydos_size{32.0F, 32.0F};
/// @brief Room kept above and below the line of a random Bydos for its weave: the
/// `amplitude` of `assets/enemies/bydos_slave.lua`.
inline constexpr float bydos_lane_margin = 48.0F;

/// @brief Enemy type the random spawns create, from `assets/enemies`.
inline constexpr std::string_view random_enemy = "bydos_slave";
/// @brief Shortest time between two random Bydos spawns, in seconds.
inline constexpr float bydos_spawn_min_interval = 0.8F;
/// @brief Longest time between two random Bydos spawns, in seconds.
inline constexpr float bydos_spawn_max_interval = 2.0F;
/// @brief x of the random spawns: a Bydos appears just beyond the right edge.
inline constexpr float bydos_spawn_x = area_width + (bydos_size.x / 2.0F);
/// @brief Highest line of a random Bydos, so that it weaves without leaving the screen.
inline constexpr float bydos_lane_top = bydos_lane_margin + (bydos_size.y / 2.0F);
/// @brief Lowest line of a random Bydos.
inline constexpr float bydos_lane_bottom = area_height - bydos_lane_top;

/// @brief Factor of the enemy health of an easy room: one missile still kills a Bydos.
inline constexpr std::int32_t easy_enemy_health_factor = 1;
/// @brief Random Bydos spawn intervals of an easy room, times the normal ones: fewer enemies.
inline constexpr float easy_spawn_interval_scale = 1.5F;
/// @brief Factor of the enemy health of a hard room: a Bydos takes two missiles.
inline constexpr std::int32_t hard_enemy_health_factor = 2;
/// @brief Random Bydos spawn intervals of a hard room, times the normal ones: more enemies.
inline constexpr float hard_spawn_interval_scale = 0.6F;

/// @brief Speed of an enemy missile, to the left.
inline constexpr float enemy_missile_speed = 300.0F;
/// @brief Size of an enemy missile.
inline constexpr aegis2d::Vec2 enemy_missile_size{8.0F, 8.0F};
/// @brief Damage of an enemy missile.
inline constexpr std::int32_t enemy_missile_damage = 1;

/// @brief Damage dealt by ramming: a player ship and a Bydos hurt each other on contact.
inline constexpr std::int32_t ram_damage = 1;

/// @brief Distance beyond the screen edges before a missile or an enemy is destroyed, so an
/// enemy whose path dips off screen is kept (`ecs::OffscreenDespawn::margin`).
inline constexpr float despawn_margin = 64.0F;
static_assert(bydos_lane_margin <= despawn_margin,
              "a Bydos weaving off screen must not be despawned before it comes back");

/// @brief Side of a cell of the collision grid, about the size of a Bydos.
inline constexpr float collision_cell = 64.0F;

} // namespace rtype::tuning
