/// @file
/// @brief The R-Type simulation run by the server: world, systems and players.
#pragma once
#include <aegis2d/core/clock.hpp>
#include <aegis2d/ecs/collision.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/damage.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/lives.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/ecs/score.hpp>
#include <aegis2d/ecs/system.hpp>

#include <rtype/tuning.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace rtype {

/// @brief One R-Type game on the server: the world, its systems in order, and the players.
/// @details The constructor registers the server systems in the order of a tick:
///
/// 1. `steering`: player ships take the velocity of their held directions;
/// 2. `movement`: every entity moves by its velocity;
/// 3. `sine_motion`: Bydos slaves weave up and down on top of it;
/// 4. `play_area`: player ships are kept on screen;
/// 5. `weapons`: players holding fire with a reloaded weapon fire a missile;
/// 6. `collision`: the grid and the layer masks give the contacts of the tick;
/// 7. `damage`: contacts remove health, consume missiles and make kills;
/// 8. `score`: each kill is credited to the player who made it;
/// 9. `lives`: killed players lose a life and respawn, or are out;
/// 10. `cleanup`: the dead and the spent missiles are destroyed;
/// 11. `despawn`: missiles and enemies that left the screen for good are destroyed.
///
/// The network layer (#112, #113, #114) calls set_input() with each client's input, tick()
/// at tuning::tick_rate, then sends the world and the events of the tick (fired(),
/// damage(), awards(), lives_lost(), despawned(), game_over()). Not thread-safe: use it from the
/// simulation thread only.
class ServerGame {
public:
    /// @brief Creates an empty game with its systems registered.
    ServerGame();
    ServerGame(const ServerGame&) = delete;
    ServerGame& operator=(const ServerGame&) = delete;
    ServerGame(ServerGame&&) = delete;
    ServerGame& operator=(ServerGame&&) = delete;
    ~ServerGame() = default;

    /// @brief Adds a player in the first free slot.
    /// @details A slot is free when it never had a player or its player is out.
    /// @return The player's ship.
    /// @throws std::length_error If tuning::max_players players are in the game.
    aegis2d::ecs::Entity add_player();

    /// @brief Slot of a player.
    /// @param player Ship returned by add_player().
    /// @return Its slot, or `std::nullopt` if it is not a player of this game any more.
    [[nodiscard]] std::optional<std::size_t> slot_of(aegis2d::ecs::Entity player) const noexcept;

    /// @brief Players still in the game.
    /// @return Ships that have lives left.
    [[nodiscard]] std::size_t players() const;

    /// @brief Sets the actions a player holds, as received from its client.
    /// @param player Ship returned by add_player().
    /// @param input Held actions; the latest one received is used at every tick.
    /// @return `false` if the player is not in the game any more (a late packet).
    bool set_input(aegis2d::ecs::Entity player, aegis2d::ecs::PlayerInput input);

    /// @brief Runs one simulation step: every system, in order.
    /// @param step Duration of the step, `core::step_from_rate(tuning::tick_rate)`.
    void tick(aegis2d::core::Duration step);

    /// @brief Tells whether every player who joined is out.
    /// @return `false` before the first player joins.
    [[nodiscard]] bool game_over() const;

    /// @brief Number of ticks run.
    /// @return Completed calls to tick().
    [[nodiscard]] std::uint64_t ticks() const noexcept;

    /// @brief World of the game, to spawn entities and to build snapshots.
    /// @return The registry.
    [[nodiscard]] aegis2d::ecs::Registry& registry() noexcept;

    /// @copydoc registry()
    [[nodiscard]] const aegis2d::ecs::Registry& registry() const noexcept;

    /// @brief Systems of the game, in update order.
    /// @return The scheduler.
    [[nodiscard]] const aegis2d::ecs::Scheduler& systems() const noexcept;

    /// @brief Missiles fired by the players during the last tick.
    /// @return The new missiles, in the order of their shooters.
    [[nodiscard]] const std::vector<aegis2d::ecs::Entity>& fired() const noexcept;

    /// @brief Hits, kills and spent missiles of the last tick.
    /// @return The damage report.
    [[nodiscard]] const aegis2d::ecs::DamageReport& damage() const noexcept;

    /// @brief Points given during the last tick.
    /// @return One award per credited kill.
    [[nodiscard]] const std::vector<aegis2d::ecs::Award>& awards() const noexcept;

    /// @brief Lives lost during the last tick.
    /// @return One event per player death.
    [[nodiscard]] const std::vector<aegis2d::ecs::LifeLost>& lives_lost() const noexcept;

    /// @brief Missiles and enemies destroyed during the last tick because they left the screen.
    /// @return The despawned entities; their handles are no longer alive.
    [[nodiscard]] const std::vector<aegis2d::ecs::Entity>& despawned() const noexcept;

    /// @brief Play area: from the origin to the logical resolution.
    /// @return The area player ships are kept in.
    [[nodiscard]] static aegis2d::ecs::Aabb play_area() noexcept;

private:
    void register_systems();

    aegis2d::ecs::Registry registry_;
    aegis2d::ecs::Scheduler systems_;
    aegis2d::ecs::CollisionGrid grid_;
    std::vector<aegis2d::ecs::Entity> shooters_;
    std::vector<aegis2d::ecs::Entity> fired_;
    std::vector<aegis2d::ecs::Contact> contacts_;
    aegis2d::ecs::DamageReport damage_;
    std::vector<aegis2d::ecs::Award> awards_;
    std::vector<aegis2d::ecs::LifeLost> lives_lost_;
    std::vector<aegis2d::ecs::Entity> despawned_;
    std::array<aegis2d::ecs::Entity, tuning::max_players> slots_{};
    bool started_ = false;
};

} // namespace rtype
