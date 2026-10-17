/// @file
/// @brief The client's copy of the server's world, built from snapshots and events.
#pragma once
#include <aegis2d/core/math.hpp>
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
#include <vector>

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

/// @brief One position of an entity, as a snapshot (or its `Spawn`) gave it.
struct TrailPoint {
    /// @brief Server tick of the position.
    std::uint32_t tick = 0;
    /// @brief Center of the entity, in logical pixels.
    aegis2d::Vec2 position{};
};

/// @brief The latest positions of an entity, oldest first, to draw it between two of them.
/// @details The server sends 20 snapshots a second, the client draws 60 frames or more:
/// drawing each entity where the last snapshot put it would make it jump three times a
/// second. ClientWorld keeps here the last positions received, and interpolate() gives the
/// position at any tick between them.
struct Trail {
    /// @brief Number of positions kept.
    static constexpr std::size_t capacity = 4;
    /// @brief Positions, oldest first; only the first `count` are set.
    std::array<TrailPoint, capacity> points{};
    /// @brief Number of positions set.
    std::size_t count = 0;

    /// @brief Adds the newest position, dropping the oldest when full.
    /// @param point Position of a tick later than the newest one; an older or equal tick is
    /// ignored.
    void push(TrailPoint point) noexcept;
};

/// @brief Position of an entity at a tick, between the positions of its trail.
/// @details Between two positions, the linear interpolation; before the oldest, the
/// oldest; after the newest, the newest (the entity is not pushed past what the server
/// said). Ticks may be fractional, the time between two ticks.
/// @param trail Positions of the entity, at least one.
/// @param tick Server tick to draw.
/// @return The position, or the origin for an empty trail.
[[nodiscard]] aegis2d::Vec2 interpolate(const Trail& trail, double tick) noexcept;

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
    /// @brief How far behind the server the remote entities are drawn: two snapshot
    /// intervals at 20 Hz, so that the tick drawn has a snapshot on each side even when one
    /// datagram is late or lost.
    static constexpr std::chrono::milliseconds interpolation_delay{100};

    /// @brief Applies an event.
    /// @param event Event from the server, read by read_event().
    /// @param now Current time, for the forgetting of destroyed ids.
    void apply(const GameEvent& event, Clock::time_point now);

    /// @brief Applies the records of one `Snapshot` datagram.
    /// @param snapshot Records read by read_snapshot().
    void apply(const Snapshot& snapshot);

    /// @brief Applies a `Snapshot` datagram and times its tick, for server_tick().
    /// @param snapshot Records read by read_snapshot().
    /// @param received When the datagram arrived.
    void apply(const Snapshot& snapshot, Clock::time_point received);

    /// @brief Server tick at a time, estimated from the snapshots received.
    /// @details Each snapshot tells the tick of the server when it left; the estimate keeps
    /// the fastest arrival seen (the least delayed) and drifts slowly towards later ones, so
    /// a late datagram does not move it back and forth.
    /// @param now Time to estimate the tick of.
    /// @return The tick, fractional, or nothing before the first snapshot.
    [[nodiscard]] std::optional<double> server_tick(Clock::time_point now) const;

    /// @brief Tick at which to draw the world: server_tick() interpolation_delay ago.
    /// @param now Time of the frame.
    /// @return The tick, or nothing before the first snapshot.
    [[nodiscard]] std::optional<double> render_tick(Clock::time_point now) const;

    /// @brief Where to draw an entity at a tick: its trail interpolated (interpolate()).
    /// @param entity Local entity of the world.
    /// @param tick Tick from render_tick().
    /// @return The position, or its `Transform` if it has no trail.
    [[nodiscard]] aegis2d::Vec2 position_at(aegis2d::ecs::Entity entity, double tick) const;

    /// @brief Events applied since the last call, in order: those ignored (late, unknown
    /// kind) are not in it.
    /// @details The client shows them: an explosion and a sound for a kill, a notice when a
    /// player leaves. Read before the player of a `PlayerLeftEvent` is gone from players():
    /// the player is removed when the event is applied.
    /// @return The events; the list is then empty.
    [[nodiscard]] std::vector<GameEvent> take_events();

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

    bool spawn(const SpawnEvent& event);
    bool destroy(const DestroyEvent& event, Clock::time_point now);
    RemotePlayer& player(std::uint8_t number);

    aegis2d::ecs::Registry registry_;
    std::unordered_map<std::uint32_t, Known> known_;
    std::array<std::optional<RemotePlayer>, tuning::max_players> players_;
    std::vector<GameEvent> applied_;
    // Estimated time of server tick 0 on the local clock: the fastest arrival seen so far.
    std::optional<Clock::time_point> tick_zero_;
};

} // namespace rtype
