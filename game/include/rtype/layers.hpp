/// @file
/// @brief Collision layers of R-Type: who can hit whom.
#pragma once
#include <cstdint>

namespace rtype {

/// @brief One bit per kind of entity, for `ecs::Collider::layer` and `ecs::Collider::mask`.
/// @details Players and their missiles never hit each other (no friendly fire), enemies
/// ignore enemies and their missiles:
///
/// | Layer | Hits (mask) |
/// |---|---|
/// | player | enemy, enemy_missile |
/// | player_missile | enemy |
/// | enemy | player, player_missile |
/// | enemy_missile | player |
namespace layer {

/// @brief A player ship.
inline constexpr std::uint32_t player = 1U << 0U;
/// @brief A missile fired by a player.
inline constexpr std::uint32_t player_missile = 1U << 1U;
/// @brief A Bydos or any other enemy ship.
inline constexpr std::uint32_t enemy = 1U << 2U;
/// @brief A missile fired by an enemy.
inline constexpr std::uint32_t enemy_missile = 1U << 3U;

} // namespace layer

/// @brief What each layer collides with.
namespace mask {

/// @brief A player ship rams enemies and is hit by their missiles.
inline constexpr std::uint32_t player = layer::enemy | layer::enemy_missile;
/// @brief A player missile only hits enemies.
inline constexpr std::uint32_t player_missile = layer::enemy;
/// @brief An enemy rams players and is hit by their missiles.
inline constexpr std::uint32_t enemy = layer::player | layer::player_missile;
/// @brief An enemy missile only hits players.
inline constexpr std::uint32_t enemy_missile = layer::player;

} // namespace mask
} // namespace rtype
