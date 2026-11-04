/// @file
/// @brief Lag compensation: a player's missiles meet the enemies where that player saw them.
#pragma once
#include <aegis2d/ecs/collision.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <rtype/position_history.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

namespace rtype {

/// @brief How many ticks back a player sees the enemies.
/// @details The enemies reach its screen half a round trip after the server moved them, then
/// wait its interpolation delay; its missile reaches the server half a round trip after it
/// fired. So the player aims at the enemies of `rtt + interpolation` ago, rounded to the
/// tick, and never further back than the history keeps (PositionHistory::capacity): a
/// slower link is not compensated beyond 500 ms.
/// @param rtt Round trip time to the player, smoothed.
/// @param interpolation How far behind the server the player's client draws the world.
/// @return 0 to PositionHistory::capacity ticks.
[[nodiscard]] std::uint32_t rewind_ticks(std::chrono::nanoseconds rtt,
                                         std::chrono::nanoseconds interpolation) noexcept;

/// @brief Finds the contacts of the player missiles with the enemies as their shooters saw
/// them.
/// @details Each entity of the `player_missile` layer with an `ecs::Owner` whose rewind is
/// above 0 is tested, at its position now, against each enemy of @p history at
/// `tick - rewind` that is still alive, with the enemy's collider. The contacts found are
/// added to @p contacts as `ecs::find_contacts()` writes them, so `ecs::apply_damage()`
/// counts them with the others; a missile that hits both now and then is consumed by the
/// first.
/// @param registry World of the game.
/// @param history Positions of the enemies at the last ticks.
/// @param tick Number the current state will have in @p history once recorded.
/// @param rewind_of Rewind of a shooter, from rewind_ticks(); 0 for no compensation.
/// @param contacts Contacts of the tick, to add to.
void find_rewound_contacts(aegis2d::ecs::Registry& registry, const PositionHistory& history,
                           std::uint64_t tick,
                           const std::function<std::uint32_t(aegis2d::ecs::Entity)>& rewind_of,
                           std::vector<aegis2d::ecs::Contact>& contacts);

} // namespace rtype
