/// @file
/// @brief The client's copy of the server's world, built from snapshots and events.
#pragma once
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <rtype/events.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace rtype {

/// @brief What the client knows of an entity of the server, next to its components.
struct Replicated {
    /// @brief Network id: the server's `Entity::id()`.
    std::uint32_t id = 0;
    /// @brief Kind, an EntityKind value.
    std::uint8_t kind = 0;
    /// @brief Player it belongs to, or no_player.
    std::uint8_t owner = no_player;
};

/// @brief What the client knows of a player of the game.
struct RemotePlayer {
    /// @brief Name from `PlayerJoined`, empty until it arrives.
    std::string name;
    /// @brief Network id of its ship, or no_entity once out of lives.
    std::uint32_t ship = no_entity;
    /// @brief Lives left.
    std::uint8_t lives = 0;
    /// @brief Score.
    std::uint32_t score = 0;
};

/// @brief The world as the server describes it (RFC sections 4.3 to 4.5).
/// @details Events create and destroy the entities: a `Spawn` creates one with a
/// `Transform`, a `Velocity`, a `Sprite` of its kind's size, a `NetworkId` and Replicated,
/// and a `Destroy` removes it. Snapshots only update them (position, velocity, frame), never
/// create or destroy one. As events and snapshots arrive in any order, the world keeps for
/// each network id the tick of its last `Spawn` or `Destroy`, and:
/// - ignores a `Spawn` or a `Destroy` older than the last one applied to the id, so a
///   `Destroy` arriving before its `Spawn` wins;
/// - ignores the snapshot records of an id it does not know, or destroyed at the record's
///   tick or later, or older than the last record applied to it;
/// - forgets a destroyed id forget_after its `Destroy`, after which the id may be reused;
/// - ignores entities of a kind it does not know.
///
/// The players come from the player records of the snapshots, `PlayerJoined`,
/// `PlayerLeft` and `Death`. Not thread-safe: use it from the client's thread.
class ClientWorld {
public:
    /// @brief Clock of the forgetting of destroyed ids.
    using Clock = std::chrono::steady_clock;

    /// @brief Time after which a destroyed id is forgotten (RFC section 4.5).
    static constexpr std::chrono::seconds forget_after{10};

    /// @brief Applies an event.
    /// @param event Event from the server, read by read_event().
    /// @param now Current time, for the forgetting of destroyed ids.
    void apply(const GameEvent& event, Clock::time_point now);

    /// @brief Applies the records of one `Snapshot` datagram.
    /// @param snapshot Records read by read_snapshot().
    void apply(const Snapshot& snapshot);

    /// @brief Records the local player, whose own `PlayerJoined` the server never sends
    /// (RFC section 4.6: it goes to the others).
    /// @param player Player number from `LoginAccepted`.
    /// @param name Name the client logged in with.
    void join(std::uint8_t player, std::string name);

    /// @brief Forgets the destroyed ids whose `Destroy` is forget_after old.
    /// @param now Current time.
    void prune(Clock::time_point now);

    /// @brief Entity of a network id.
    /// @param id Network id.
    /// @return The local entity, or `std::nullopt` if the id is unknown or destroyed.
    [[nodiscard]] std::optional<aegis2d::ecs::Entity> entity(std::uint32_t id) const;

    /// @brief Number of entities alive.
    /// @return The count.
    [[nodiscard]] std::size_t entities() const noexcept;

    /// @brief The players of the game, by number.
    /// @return An empty slot for a number with no player.
    [[nodiscard]] const std::array<std::optional<RemotePlayer>, tuning::max_players>&
    players() const noexcept;

    /// @brief The world, to draw it.
    /// @return The registry of the local entities.
    [[nodiscard]] aegis2d::ecs::Registry& registry() noexcept;

    /// @copydoc registry()
    [[nodiscard]] const aegis2d::ecs::Registry& registry() const noexcept;

private:
    struct Known {
        std::optional<aegis2d::ecs::Entity> entity;
        // Tick of its last Spawn or Destroy applied, and of its last snapshot record.
        std::uint32_t event_tick = 0;
        std::uint32_t record_tick = 0;
        bool destroyed = false;
        Clock::time_point destroyed_at;
    };

    void spawn(const SpawnEvent& event);
    void destroy(const DestroyEvent& event, Clock::time_point now);
    RemotePlayer& player(std::uint8_t number);

    aegis2d::ecs::Registry registry_;
    std::unordered_map<std::uint32_t, Known> known_;
    std::array<std::optional<RemotePlayer>, tuning::max_players> players_;
};

} // namespace rtype
