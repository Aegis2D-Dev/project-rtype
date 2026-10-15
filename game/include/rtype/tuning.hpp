/// @file
/// @brief Gameplay values of R-Type: play area, speeds, sizes, health, lives and scores.
#pragma once
#include <aegis2d/core/math.hpp>

#include <cstddef>
#include <cstdint>

/// @brief Values tuned for the game, in logical pixels, seconds and health points.
namespace rtype::tuning {

/// @brief Simulation rate of the server, in ticks per second.
inline constexpr std::uint32_t tick_rate = 60;

/// @brief Snapshots the server sends each client per second (RFC section 4.3).
inline constexpr std::uint32_t snapshot_rate = 20;
static_assert(tick_rate % snapshot_rate == 0, "a snapshot every whole number of ticks");

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

/// @brief Speed of a Bydos slave, to the left.
inline constexpr float bydos_speed = 120.0F;
/// @brief Size of a Bydos slave.
inline constexpr aegis2d::Vec2 bydos_size{32.0F, 32.0F};
/// @brief Health points of a Bydos slave: one missile kills it.
inline constexpr std::int32_t bydos_health = 1;
/// @brief Points for killing a Bydos slave.
inline constexpr std::uint32_t bydos_score = 100;
/// @brief Largest distance of a Bydos slave above or below its line as it weaves.
inline constexpr float bydos_wave_amplitude = 48.0F;
/// @brief Seconds for a Bydos slave to weave up, down and back to its line.
inline constexpr float bydos_wave_period = 2.0F;
/// @brief Seconds between two shots of a Bydos slave.
inline constexpr float bydos_fire_cooldown = 2.0F;
/// @brief Shortest time before the first shot of a Bydos slave, so that it fires once on
/// screen (it enters about 0.3 s after spawning beyond the right edge). Random Bydos wait up
/// to one more tuning::bydos_fire_cooldown, so they do not fire in step.
inline constexpr float bydos_first_shot = 1.0F;

/// @brief Shortest time between two random Bydos spawns, in seconds.
inline constexpr float bydos_spawn_min_interval = 0.8F;
/// @brief Longest time between two random Bydos spawns, in seconds.
inline constexpr float bydos_spawn_max_interval = 2.0F;
/// @brief x of the random spawns: a Bydos appears just beyond the right edge.
inline constexpr float bydos_spawn_x = area_width + (bydos_size.x / 2.0F);
/// @brief Highest line of a random Bydos, so that it weaves without leaving the screen.
inline constexpr float bydos_lane_top = bydos_wave_amplitude + (bydos_size.y / 2.0F);
/// @brief Lowest line of a random Bydos.
inline constexpr float bydos_lane_bottom = area_height - bydos_lane_top;

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
static_assert(bydos_wave_amplitude <= despawn_margin,
              "a Bydos weaving off screen must not be despawned before it comes back");

/// @brief Side of a cell of the collision grid, about the size of a Bydos.
inline constexpr float collision_cell = 64.0F;

} // namespace rtype::tuning
