/// @file
/// @brief The state of the server's game, as the snapshots of RFC section 4.3 carry it.
#pragma once
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>

#include <cstdint>
#include <optional>

namespace rtype {

class GameInstance;

/// @brief Kind of the entities of a collision layer, for snapshots and events.
/// @details Each layer holds one kind of entity for now.
/// @param layer `ecs::Collider::layer`, one of the values of `rtype::layer`.
/// @return The kind, or `std::nullopt` for a layer whose entities are not replicated.
[[nodiscard]] std::optional<EntityKind> kind_of_layer(std::uint32_t layer) noexcept;

/// @brief Tells whether a snapshot is due after a tick.
/// @details tuning::snapshot_rate times per second: every third tick at 60 ticks per second.
/// @param tick Number of ticks run, `ServerGame::ticks()`.
/// @return `true` every tuning::tick_rate / tuning::snapshot_rate ticks.
[[nodiscard]] constexpr bool snapshot_due(std::uint64_t tick) noexcept {
    return tick % (tuning::tick_rate / tuning::snapshot_rate) == 0;
}

/// @brief Takes the state of the game, on the simulation thread.
/// @details Players in their number order; entities with a `NetworkId`, a `Transform` and
/// a `Collider` whose layer gives their kind (entities of another layer are left out).
/// @param instance Game to read; the scores of players out of lives are the last ones seen.
/// @return The snapshot.
[[nodiscard]] Snapshot take_snapshot(GameInstance& instance);

} // namespace rtype
