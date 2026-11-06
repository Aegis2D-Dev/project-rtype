/// @file
/// @brief The R-Type simulation run by the server: world, systems and players.
#pragma once
#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/collision.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/damage.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/lives.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/ecs/score.hpp>
#include <aegis2d/ecs/spawner.hpp>
#include <aegis2d/ecs/system.hpp>
#include <aegis2d/ecs/waves.hpp>

#include <rtype/position_history.hpp>
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
/// 1. `steering`: player ships take the velocity of their held directions (steer_ships());
/// 2. `movement`: every entity moves by its velocity;
/// 3. `sine_motion`: Bydos slaves weave up and down on top of it;
/// 4. `play_area`: player ships are kept on screen (keep_ships_in_area());
/// 5. `weapons`: players holding fire with a reloaded weapon fire a missile;
/// 6. `enemy_fire`: enemies on screen with a reloaded weapon fire a missile to the left;
/// 7. `spawn`: Bydos slaves appear at random on the right edge (only with a seed);
/// 8. `waves`: the enemies of the waves given to play_waves() appear when due;
/// 9. `collision`: the grid and the layer masks give the contacts of the tick;
/// 10. `damage`: contacts remove health, consume missiles and make kills;
/// 11. `score`: each kill is credited to the player who made it;
/// 12. `lives`: killed players lose a life and respawn, or are out;
/// 13. `cleanup`: the dead and the spent missiles are destroyed;
/// 14. `despawn`: missiles and enemies that left the screen for good are destroyed.
///
/// Systems 1, 2 and 4 move a player ship as step_ship() does, which the client runs to
/// predict its own ship.
///
/// The network layer (#112, #113, #114) creates it with a seed, gives it the waves of the
/// level (play_waves(load_waves(...))), calls set_input() with each
/// client's input, tick() at tuning::tick_rate, then sends the world and the events of the
/// tick (fired(), enemy_fired(), spawned(), damage(), awards(), lives_lost(), despawned(),
/// game_over()). Not thread-safe: use it from the simulation thread only.
class ServerGame {
public:
    /// @brief Creates an empty game with its systems registered.
    /// @param seed Seed of the random Bydos spawns: the same seed gives the same spawns, so
    /// a game can be replayed from its logged seed. Without one, the `spawn` system is
    /// disabled and only what the caller creates is in the world (tests, scripted levels).
    explicit ServerGame(std::optional<std::uint64_t> seed = std::nullopt);
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

    /// @brief Adds a player in a given slot, the player number of its session.
    /// @param slot Slot, 0 to tuning::max_players - 1; it picks the spawn point.
    /// @return The player's ship.
    /// @throws std::out_of_range If @p slot is not below tuning::max_players.
    /// @throws std::logic_error If the slot has a player still in the game.
    aegis2d::ecs::Entity add_player(std::size_t slot);

    /// @brief Removes a player who left: its ship is destroyed and its slot is free.
    /// @details The game goes on for the others; once every player has left or is out,
    /// game_over() is `true`.
    /// @param player Ship returned by add_player().
    /// @return `false` if it is not a player of this game any more.
    bool remove_player(aegis2d::ecs::Entity player);

    /// @brief Slot of a player.
    /// @param player Ship returned by add_player().
    /// @return Its slot, or `std::nullopt` if it is not a player of this game any more.
    [[nodiscard]] std::optional<std::size_t> slot_of(aegis2d::ecs::Entity player) const noexcept;

    /// @brief Players still in the game.
    /// @return Ships that have lives left.
    [[nodiscard]] std::size_t players() const;

    /// @brief Plays enemy waves from now on, replacing the ones played so far.
    /// @param waves Schedule read by load_waves(), at its start.
    void play_waves(aegis2d::ecs::WaveSchedule waves);

    /// @brief Enemy waves being played.
    /// @return The schedule given to play_waves(), or an empty (finished) one.
    [[nodiscard]] const aegis2d::ecs::WaveSchedule& waves() const noexcept;

    /// @brief Sets the actions a player holds, as received from its client.
    /// @param player Ship returned by add_player().
    /// @param input Held actions; the latest one received is used at every tick.
    /// @return `false` if the player is not in the game any more (a late packet).
    bool set_input(aegis2d::ecs::Entity player, aegis2d::ecs::PlayerInput input);

    /// @brief Sets how many ticks back a player's missiles meet the enemies (lag
    /// compensation): its rewind_ticks(), from its round trip time.
    /// @param player Ship returned by add_player().
    /// @param ticks 0 for none, at most PositionHistory::capacity.
    /// @return `false` if the player is out.
    bool set_rewind(aegis2d::ecs::Entity player, std::uint32_t ticks);

    /// @brief How many ticks back a player's missiles meet the enemies.
    /// @param player Ship returned by add_player().
    /// @return The rewind, 0 for a player out or never set.
    [[nodiscard]] std::uint32_t rewind(aegis2d::ecs::Entity player) const;

    /// @brief Runs one simulation step: every system, in order.
    /// @param step Duration of the step, `core::step_from_rate(tuning::tick_rate)`.
    void tick(aegis2d::core::Duration step);

    /// @brief Tells whether every player who joined is out.
    /// @return `false` before the first player joins.
    [[nodiscard]] bool game_over() const;

    /// @brief Number of ticks run.
    /// @return Completed calls to tick().
    [[nodiscard]] std::uint64_t ticks() const noexcept;

    /// @brief Where the enemies were at each of the last 500 ms of ticks.
    /// @details Recorded at the end of each tick(), under the number ticks() then has.
    /// @return The history.
    [[nodiscard]] const PositionHistory& history() const noexcept;

    /// @brief World of the game, to spawn entities and to build snapshots.
    /// @return The registry.
    [[nodiscard]] aegis2d::ecs::Registry& registry() noexcept;

    /// @copydoc registry()
    [[nodiscard]] const aegis2d::ecs::Registry& registry() const noexcept;

    /// @brief Times each system of the game with a profiler (`ecs::Scheduler::set_profiler()`).
    /// @param profiler Profiler of the simulation thread, or `nullptr` for none.
    void set_profiler(aegis2d::core::Profiler* profiler) noexcept;

    /// @brief Systems of the game, in update order.
    /// @return The scheduler.
    [[nodiscard]] const aegis2d::ecs::Scheduler& systems() const noexcept;

    /// @brief Missiles fired by the players during the last tick.
    /// @return The new missiles, in the order of their shooters.
    [[nodiscard]] const std::vector<aegis2d::ecs::Entity>& fired() const noexcept;

    /// @brief Missiles fired by the enemies during the last tick.
    /// @return The new missiles, in the order of their shooters.
    [[nodiscard]] const std::vector<aegis2d::ecs::Entity>& enemy_fired() const noexcept;

    /// @brief Enemies spawned during the last tick, at random or by the waves.
    /// @return The new enemies, random ones first, in spawn order.
    [[nodiscard]] const std::vector<aegis2d::ecs::Entity>& spawned() const noexcept;

    /// @brief Seed of the random spawns.
    /// @return The seed given to the constructor, or `std::nullopt` without random spawns.
    [[nodiscard]] std::optional<std::uint64_t> seed() const noexcept;

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
    std::vector<aegis2d::ecs::Entity> gunners_;
    std::vector<aegis2d::ecs::Entity> enemy_fired_;
    std::optional<std::uint64_t> seed_;
    aegis2d::ecs::RandomSpawner spawner_;
    std::vector<aegis2d::Vec2> spawn_points_;
    std::vector<aegis2d::ecs::Entity> spawned_;
    aegis2d::ecs::WaveSchedule waves_;
    std::vector<aegis2d::ecs::WaveSpawn> due_;
    std::vector<aegis2d::ecs::Contact> contacts_;
    aegis2d::ecs::DamageReport damage_;
    std::vector<aegis2d::ecs::Award> awards_;
    std::vector<aegis2d::ecs::LifeLost> lives_lost_;
    std::vector<aegis2d::ecs::Entity> despawned_;
    std::array<aegis2d::ecs::Entity, tuning::max_players> slots_{};
    PositionHistory history_;
    std::array<std::uint32_t, tuning::max_players> rewinds_{};
    bool started_ = false;
};

} // namespace rtype
