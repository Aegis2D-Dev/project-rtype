/// @file
/// @brief The movement of a player ship for one tick, the same on the server and the client.
#pragma once
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/ecs/system.hpp>

namespace rtype {

/// @brief Where a player ship is and how it moves, after a step.
struct ShipMotion {
    /// @brief Center of the ship, in logical pixels.
    aegis2d::Vec2 position;
    /// @brief Velocity, in logical pixels per second.
    aegis2d::Vec2 velocity;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const ShipMotion&) const = default;
};

/// @brief Velocity of a player ship holding an input.
/// @details Its held directions at tuning::player_speed, diagonals as fast as straight lines
/// (`ecs::direction_of()`); opposite directions cancel out.
/// @param input Held actions.
/// @return The velocity, zero when no direction is held.
[[nodiscard]] aegis2d::Vec2 ship_velocity(aegis2d::ecs::PlayerInput input) noexcept;

/// @brief Keeps a player ship inside the play area.
/// @details Its box (tuning::player_size) must fit in ServerGame::play_area(): holding a
/// direction against an edge stops the ship there, and it can leave the edge at once.
/// @param position Center of the ship.
/// @return The nearest position whose box fits.
[[nodiscard]] aegis2d::Vec2 keep_ship_in_area(aegis2d::Vec2 position) noexcept;

/// @brief Moves a player ship by one tick.
/// @details The velocity of its input (ship_velocity()), the position moved by it for
/// @p delta seconds, then kept inside the play area (keep_ship_in_area()): what the server's
/// `steering`, `movement` and `play_area` systems do to the ship, with the same arithmetic,
/// so a client that runs it on its inputs finds the server's positions.
/// @param position Center of the ship before the tick.
/// @param input Actions held during the tick.
/// @param delta Duration of the tick in seconds, `ecs::Tick::delta()`.
/// @return The position and velocity after the tick.
[[nodiscard]] ShipMotion step_ship(aegis2d::Vec2 position, aegis2d::ecs::PlayerInput input,
                                   float delta) noexcept;

/// @brief System that sets the velocity of every player ship from its input.
/// @details ship_velocity() for each entity with `shmup::PlayerShip`, `ecs::PlayerInput` and
/// `ecs::Velocity`; `ecs::apply_velocity()` then moves it.
/// @param registry World holding the ships.
/// @param tick Unused: the speed is per second.
void steer_ships(aegis2d::ecs::Registry& registry, const aegis2d::ecs::Tick& tick);

/// @brief System that keeps every player ship inside the play area.
/// @details keep_ship_in_area() for each entity with `shmup::PlayerShip` and
/// `ecs::Transform`; run it after `ecs::apply_velocity()`.
/// @param registry World holding the ships.
/// @param tick Unused.
void keep_ships_in_area(aegis2d::ecs::Registry& registry, const aegis2d::ecs::Tick& tick);

} // namespace rtype
