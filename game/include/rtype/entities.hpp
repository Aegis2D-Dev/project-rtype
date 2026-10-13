/// @file
/// @brief Creation of the R-Type entities with their components.
#pragma once
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <cstddef>

/// @brief R-Type game code, built on the Aegis2D engine.
namespace rtype {

/// @brief Where a player slot starts and respawns.
/// @details Slots are spread over the height of the play area, at tuning::player_spawn_x.
/// @param slot Player slot, from 0 to tuning::max_players - 1.
/// @return Center of the ship.
/// @throws std::out_of_range If @p slot is not a valid slot.
[[nodiscard]] aegis2d::Vec2 player_spawn(std::size_t slot);

/// @brief Creates the ship of a player.
/// @details The ship is steered by its `ecs::PlayerInput`, kept in the play area, fires
/// every tuning::player_fire_cooldown seconds while fire is held, has one health point,
/// tuning::player_lives lives respawning at player_spawn(), a `Score`, a `NetworkId`, and
/// rams enemies.
/// @param registry World of the game.
/// @param slot Player slot, from 0 to tuning::max_players - 1.
/// @return The ship.
/// @throws std::out_of_range If @p slot is not a valid slot.
aegis2d::ecs::Entity spawn_player(aegis2d::ecs::Registry& registry, std::size_t slot);

/// @brief Creates a missile fired by a player, in front of its ship.
/// @details The missile flies right, hits only enemies, is consumed by its first hit,
/// credits its kill to @p shooter (`ecs::Owner`) and is destroyed once past the right edge
/// (`ecs::OffscreenDespawn`).
/// @param registry World of the game.
/// @param shooter Player ship that fires; it must have a Transform.
/// @return The missile.
/// @throws std::out_of_range If @p shooter has no Transform.
aegis2d::ecs::Entity spawn_player_missile(aegis2d::ecs::Registry& registry,
                                          aegis2d::ecs::Entity shooter);

/// @brief Creates a Bydos slave, the first enemy type.
/// @details It flies left while weaving up and down around its line
/// (tuning::bydos_wave_amplitude, every tuning::bydos_wave_period seconds, `ecs::SineMotion`),
/// dies from one missile, gives tuning::bydos_score points to its killer, rams players,
/// ignores the other enemies and is destroyed once it has crossed the screen; it can be
/// created beyond the right edge.
/// @param registry World of the game.
/// @param position Center of the Bydos, on its line: it first rises (towards y = 0) and
/// comes back to it.
/// @return The Bydos.
aegis2d::ecs::Entity spawn_bydos_slave(aegis2d::ecs::Registry& registry, aegis2d::Vec2 position);

/// @brief Creates a missile fired by an enemy.
/// @details It flies left, hits only players, is consumed by its first hit and is destroyed
/// once past the left edge.
/// @param registry World of the game.
/// @param position Center of the missile.
/// @return The missile.
aegis2d::ecs::Entity spawn_enemy_missile(aegis2d::ecs::Registry& registry, aegis2d::Vec2 position);

} // namespace rtype
