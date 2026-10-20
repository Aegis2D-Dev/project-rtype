/// @file
/// @brief Creation of the R-Type entities with their components.
#pragma once
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <rtype/tuning.hpp>

#include <cstddef>
#include <string_view>

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
/// fires on its own every tuning::bydos_fire_cooldown seconds (`shmup::Weapon` and
/// `shmup::AutoTrigger`; see fire_enemy_missile()), dies from one missile, gives
/// tuning::bydos_score points to its killer, rams players, ignores the other enemies and is
/// destroyed once it has crossed the screen; it can be created beyond the right edge.
/// @param registry World of the game.
/// @param line Point of the line it weaves around; with no @p phase, its center, and it
/// first rises (towards y = 0) and comes back to it.
/// @param phase Seconds into its wave, from 0 to tuning::bydos_wave_period, so that Bydos
/// spawned together do not move in step; the Bydos is placed where its wave is at that
/// time.
/// @param first_shot Seconds before its first shot.
/// @return The Bydos.
aegis2d::ecs::Entity spawn_bydos_slave(aegis2d::ecs::Registry& registry, aegis2d::Vec2 line,
                                       float phase = 0.0F,
                                       float first_shot = tuning::bydos_first_shot);

/// @brief Creates a missile fired by an enemy.
/// @details It flies left, hits only players, is consumed by its first hit and is destroyed
/// once past the left edge.
/// @param registry World of the game.
/// @param position Center of the missile.
/// @return The missile.
aegis2d::ecs::Entity spawn_enemy_missile(aegis2d::ecs::Registry& registry, aegis2d::Vec2 position);

/// @brief Creates the missile an enemy fires, in front of it (on its left).
/// @details A spawn_enemy_missile() just left of the enemy's sprite (or of its center without
/// one), with `ecs::Owner` set to the enemy.
/// @param registry World of the game.
/// @param shooter Enemy that fires; it must have a Transform.
/// @return The missile.
/// @throws std::out_of_range If @p shooter has no Transform.
aegis2d::ecs::Entity fire_enemy_missile(aegis2d::ecs::Registry& registry,
                                        aegis2d::ecs::Entity shooter);

/// @brief Creates an enemy by the name a waves file gives it (see enemy_types).
/// @details `"bydos_slave"` is spawn_bydos_slave() with @p position on its line.
/// @param registry World of the game.
/// @param type Name of the enemy type.
/// @param position Where it appears.
/// @return The enemy.
/// @throws std::invalid_argument If @p type is not one of enemy_types.
aegis2d::ecs::Entity spawn_enemy(aegis2d::ecs::Registry& registry, std::string_view type,
                                 aegis2d::Vec2 position);

} // namespace rtype
