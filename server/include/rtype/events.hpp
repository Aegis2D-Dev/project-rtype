/// @file
/// @brief Gameplay events sent to the clients (RFC section 4.4): spawns, destructions,
/// deaths, and the players who join and leave.
#pragma once
#include <aegis2d/ecs/entity.hpp>

#include <rtype/snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace rtype {

class GameInstance;

/// @brief Type of an event, its first byte (RFC section 4.4).
/// @details The values are part of the protocol: never renumber them.
enum class EventType : std::uint8_t {
    /// @brief An entity appears; a missile's is also its shot.
    Spawn = 1,
    /// @brief An entity disappears.
    Destroy = 2,
    /// @brief A player lost a life.
    Death = 3,
    /// @brief A player joined the game.
    PlayerJoined = 4,
    /// @brief A player left the game.
    PlayerLeft = 5,
    /// @brief No player has lives left.
    GameOver = 6,
};

/// @brief Why an entity was destroyed, which tells the client what to show.
/// @details The values are part of the protocol: never renumber them.
enum class DestroyReason : std::uint8_t {
    /// @brief It left the play area: no effect.
    Despawned = 0,
    /// @brief It was destroyed: an explosion.
    Killed = 1,
    /// @brief A missile that hit its target: it vanishes.
    Consumed = 2,
    /// @brief The ship of a player who left the game.
    PlayerLeft = 3,
};

/// @brief Why a player left the game, in `PlayerLeft`.
/// @details The values are part of the protocol: never renumber them.
enum class LeaveReason : std::uint8_t {
    /// @brief It quit: `Logout`, `Disconnect`, or its TCP connection closed.
    Left = 0,
    /// @brief No valid datagram for too long, or no event acknowledged (RFC section 5.5).
    TimedOut = 1,
    /// @brief A protocol error ended its session (RFC section 3.2).
    Kicked = 2,
};

/// @brief Player number meaning "no player", as the `owner` of an enemy.
inline constexpr std::uint8_t no_player = 255;

/// @brief Size of a `Spawn` event, type and tick included.
inline constexpr std::size_t spawn_event_size = 27;
/// @brief Size of a `Destroy` event, type and tick included.
inline constexpr std::size_t destroy_event_size = 18;
/// @brief Size of a `Death` event, type and tick included.
inline constexpr std::size_t death_event_size = 15;
/// @brief Size of a `PlayerLeft` event, type and tick included.
inline constexpr std::size_t player_left_event_size = 7;

/// @brief An entity appears, with its kind, position and velocity.
struct SpawnEvent {
    /// @brief Server tick at which it appeared.
    std::uint32_t tick = 0;
    /// @brief Network id.
    std::uint32_t id = 0;
    /// @brief Kind, an EntityKind value.
    std::uint8_t kind = 0;
    /// @brief Position of its center, in logical pixels.
    float x = 0.0F;
    /// @copydoc x
    float y = 0.0F;
    /// @brief Velocity, in logical pixels per second.
    float vx = 0.0F;
    /// @copydoc vx
    float vy = 0.0F;
    /// @brief Player it belongs to (a ship's player, a missile's shooter), or no_player.
    std::uint8_t owner = no_player;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const SpawnEvent&) const = default;
};

/// @brief An entity disappears, at its last position.
struct DestroyEvent {
    /// @brief Server tick at which it disappeared.
    std::uint32_t tick = 0;
    /// @brief Network id.
    std::uint32_t id = 0;
    /// @brief Why, which tells the client what to show.
    DestroyReason reason = DestroyReason::Killed;
    /// @brief Last position of its center, in logical pixels.
    float x = 0.0F;
    /// @copydoc x
    float y = 0.0F;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const DestroyEvent&) const = default;
};

/// @brief A player lost a life where its ship was.
struct DeathEvent {
    /// @brief Server tick of the death.
    std::uint32_t tick = 0;
    /// @brief Player number, 0 to 3.
    std::uint8_t player = 0;
    /// @brief Lives left; 0 means the player is out, and a `Destroy` of its ship follows.
    std::uint8_t lives = 0;
    /// @brief Position of the ship when it was hit, in logical pixels.
    float x = 0.0F;
    /// @copydoc x
    float y = 0.0F;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const DeathEvent&) const = default;
};

/// @brief A player joined the game.
struct PlayerJoinedEvent {
    /// @brief Server tick at which it joined.
    std::uint32_t tick = 0;
    /// @brief Player number, 0 to 3.
    std::uint8_t player = 0;
    /// @brief Name given at login, valid as valid_player_name() says.
    std::string name;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const PlayerJoinedEvent&) const = default;
};

/// @brief A player left the game; the `Destroy` of its ship follows if it had one.
struct PlayerLeftEvent {
    /// @brief Server tick at which it left.
    std::uint32_t tick = 0;
    /// @brief Player number, 0 to 3.
    std::uint8_t player = 0;
    /// @brief Why it left.
    LeaveReason reason = LeaveReason::Left;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const PlayerLeftEvent&) const = default;
};

/// @brief One of the gameplay events the server sends.
using GameEvent =
    std::variant<SpawnEvent, DestroyEvent, DeathEvent, PlayerJoinedEvent, PlayerLeftEvent>;

/// @brief Encodes an event as the bytes of RFC section 4.4: type, tick, then its fields.
/// @param event Event to encode.
/// @return spawn_event_size, destroy_event_size, death_event_size or
/// player_left_event_size bytes; 8 plus the name for `PlayerJoined`.
[[nodiscard]] std::vector<std::byte> write_event(const GameEvent& event);

/// @brief Encodes events with write_event(), for ServerNetwork::send_events().
/// @param events Events to encode, in order.
/// @return Their bytes, in the same order.
[[nodiscard]] std::vector<std::vector<std::byte>> write_events(std::span<const GameEvent> events);

/// @brief Decodes the bytes of one event, as a client does (RFC section 6.4).
/// @param bytes Bytes of the event, as handed over by `ReliableChannel::read_events()`.
/// @return The event, or `std::nullopt` if its type is not one of the five above or its
/// bytes do not match the type: a wrong size, a non-finite position or velocity, an
/// `owner` other than 0 to 3 and 255, a `player` above 3, or a name that breaks the rules
/// of valid_player_name(). Unknown kinds and reasons are kept, for the caller to apply its
/// rules.
[[nodiscard]] std::optional<GameEvent> read_event(std::span<const std::byte> bytes);

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
