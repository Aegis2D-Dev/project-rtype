/// @file
/// @brief What changed in the server's game at each tick, as the events of RFC section 4.4.
#pragma once
#include <aegis2d/ecs/entity.hpp>

#include <rtype/events.hpp>
#include <rtype/snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace rtype {

class GameInstance;

/// @brief Finds what changed in the game at each tick, as events.
/// @details Keeps the replicated entities of the game (those of the snapshots) from one
/// tick to the next. An entity that appeared is a `Spawn`: a ship, owned by its player, an
/// enemy, or a missile, owned by its shooter's player, which is the shot itself. An entity
/// that disappeared is a `Destroy` at its last position, whose reason comes from the tick's
/// reports: `Killed` if its health ran out, `Consumed` for a missile spent on its hit,
/// `Despawned` if it left the screen, `PlayerLeft` for the ship of a player who left. A
/// life lost is a `Death` where the ship was, before the `Destroy` of the ship when it was
/// the last life.
///
/// An entity created and destroyed within one tick is never seen. When the instance
/// starts a new game, the entities of the old one are forgotten without events. Use it
/// from the simulation thread.
class EventTracker {
public:
    /// @brief Events of the last tick: what changed since the previous call.
    /// @details Call it after each tick, before the next one. Spawns come first, then
    /// deaths, then destructions.
    /// @param instance Game that just ticked.
    /// @return The events, possibly none.
    [[nodiscard]] std::vector<GameEvent> update(GameInstance& instance);

    /// @brief A `Spawn` of every entity alive, for a player who joins (RFC section 4.6).
    /// @details The entities known at the last update(): an entity created since, such as
    /// the ship of the player who joins, comes with the next update().
    /// @param instance Game being played.
    /// @return The events, at the current tick, with the entities' current positions.
    [[nodiscard]] std::vector<GameEvent> spawns(GameInstance& instance);

    /// @brief Entities followed.
    /// @return The count.
    [[nodiscard]] std::size_t tracked() const noexcept;

private:
    struct Tracked {
        std::uint32_t id = 0;
        EntityKind kind = EntityKind::BydosSlave;
        float x = 0.0F;
        float y = 0.0F;
        std::uint8_t owner = no_player;
    };

    void follow(GameInstance& instance);

    std::unordered_map<aegis2d::ecs::Entity, Tracked> entities_;
    std::uint64_t game_ = 0;
};

} // namespace rtype
