/// @file
/// @brief Where the enemies were at each of the last ticks, for lag compensation.
#pragma once
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

namespace rtype {

/// @brief The positions of the enemies over the last tuning::position_history_ticks ticks.
/// @details record() keeps, after a tick, the center of every entity of the `enemy`
/// collision layer; position() gives back where one was at a past tick. A player's missiles
/// can then be checked against the enemies as that player saw them, its round trip time
/// and interpolation delay ago (lag compensation). Older ticks are forgotten. Not
/// thread-safe: use it from the simulation thread.
class PositionHistory {
public:
    /// @brief Ticks kept: 500 ms at 60 ticks per second.
    static constexpr std::size_t capacity = tuning::position_history_ticks;

    /// @brief Keeps the positions of the enemies after a tick.
    /// @details A tick not newer than the last one recorded starts the history again (a new
    /// game); beyond capacity, the oldest tick is forgotten.
    /// @param tick Number of the tick, `ServerGame::ticks()` once it ran.
    /// @param registry World of the game: every entity with an `ecs::Collider` of
    /// `layer::enemy` and an `ecs::Transform`. Only read, but `Registry::view()` needs it
    /// mutable.
    void record(std::uint64_t tick, aegis2d::ecs::Registry& registry);

    /// @brief Where an enemy was at a past tick.
    /// @param entity The enemy, as its handle was then (a reused slot is another entity).
    /// @param tick Tick to read, one of the last capacity recorded.
    /// @return Its center, or nothing if the tick is not kept or the enemy was not there.
    [[nodiscard]] std::optional<aegis2d::Vec2> position(aegis2d::ecs::Entity entity,
                                                        std::uint64_t tick) const;

    /// @brief The enemies at a past tick.
    /// @param tick Tick to read.
    /// @return Their handles and centers, by increasing `Entity::id()`; empty if the tick is
    /// not kept.
    [[nodiscard]] std::vector<std::pair<aegis2d::ecs::Entity, aegis2d::Vec2>>
    enemies(std::uint64_t tick) const;

    /// @brief Oldest tick kept.
    /// @return It, or nothing before the first record().
    [[nodiscard]] std::optional<std::uint64_t> oldest() const noexcept;

    /// @brief Newest tick kept.
    /// @return It, or nothing before the first record().
    [[nodiscard]] std::optional<std::uint64_t> newest() const noexcept;

    /// @brief Number of ticks kept.
    /// @return 0 to capacity.
    [[nodiscard]] std::size_t size() const noexcept;

    /// @brief Forgets every tick.
    void clear() noexcept;

private:
    struct Frame {
        std::uint64_t tick = 0;
        // By increasing Entity::id(), for a binary search.
        std::vector<std::pair<aegis2d::ecs::Entity, aegis2d::Vec2>> enemies;
    };

    [[nodiscard]] const Frame* frame(std::uint64_t tick) const noexcept;

    std::deque<Frame> frames_;
};

} // namespace rtype
