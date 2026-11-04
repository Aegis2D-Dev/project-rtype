/// @file
/// @brief The client's own ship, moved at once by its inputs (client-side prediction).
#pragma once
#include <aegis2d/core/math.hpp>

#include <rtype/input.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace rtype {

/// @brief Where the client's own ship is, before the server says so.
/// @details Starts from a position of the ship the server sent (start()); each input the
/// client sends then moves it at once with step_ship(), the server's own movement, and is
/// kept until a snapshot acknowledges it (acknowledge()): the inputs the server has not
/// applied yet, which server reconciliation replays. The client draws its ship at
/// position() instead of interpolating it 100 ms in the past, so it answers the keys within
/// a frame whatever the round trip time. Not thread-safe: use it from the client's thread.
class ShipPrediction {
public:
    /// @brief Inputs kept at most: 2 seconds at 60 per second. Older ones are dropped, as no
    /// snapshot acknowledges them on a link that bad.
    static constexpr std::size_t max_pending = 120;

    /// @brief Distance from which a reconciliation counts as a correction, in logical pixels.
    /// @details Snapshots round positions to 1/8 pixel (RFC section 4.3): two positions of the
    /// same path, each rounded, may differ by up to 1/8 pixel on each axis without any error
    /// of the prediction.
    static constexpr float correction_threshold = 0.25F;

    /// @brief Speed at which the drawn ship catches up with a correction, in logical pixels
    /// per second: 2 pixels a frame at 60 frames per second, so a correction of a few pixels
    /// takes a few frames instead of a jump.
    static constexpr float smoothing_speed = 120.0F;

    /// @brief Corrections from this distance are not smoothed: the ship is far from where it
    /// was predicted (a hit, a long stall), and gliding there would show it in the wrong place
    /// for long.
    static constexpr float snap_distance = 32.0F;

    /// @brief Starts predicting a ship from a position of the server.
    /// @details Forgets the inputs kept: the server's position already holds them.
    /// @param ship Network id of the ship.
    /// @param position Center of the ship, from a snapshot or its `Spawn`.
    void start(std::uint32_t ship, aegis2d::Vec2 position);

    /// @brief Stops predicting: the ship is gone, or the player out of lives.
    void stop() noexcept;

    /// @brief Moves the ship by an input the client sends, and keeps the input.
    /// @details Does nothing before start(). The oldest input is dropped beyond max_pending.
    /// @param input The input, numbered as in its `Input` datagram.
    /// @param delta Duration of a tick in seconds, as the server steps.
    void apply(const InputRecord& input, float delta);

    /// @brief Drops the inputs the server applied.
    /// @param sequence The last input applied, from the player record of a snapshot; the
    /// inputs up to it, by `aegis2d::net::sequence_greater_than()`, are dropped.
    void acknowledge(std::uint16_t sequence);

    /// @brief Corrects the prediction with a snapshot (server reconciliation).
    /// @details Drops the inputs the snapshot acknowledges, puts the ship where the server
    /// had it once it applied them, then replays the inputs kept, each with step_ship(), as
    /// apply() did: when the server applied the inputs as the client predicted, the ship ends
    /// where it was. Does nothing before start().
    /// @param server Position of the ship in the snapshot.
    /// @param acknowledged The last input of the player the server applied, from its player
    /// record; nothing when it has applied none, so every input kept is replayed.
    /// @param delta Duration of a tick in seconds, as given to apply().
    /// @return How far the ship moved, 0 when the prediction was right.
    float reconcile(aegis2d::Vec2 server, std::optional<std::uint16_t> acknowledged, float delta);

    /// @brief Number of reconciliations that moved the ship by more than
    /// correction_threshold: the prediction was wrong.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t corrections() const noexcept;

    /// @brief How far the last reconciliation moved the ship.
    /// @return The distance in logical pixels, 0 before the first.
    [[nodiscard]] float last_correction() const noexcept;

    /// @brief Brings the drawn ship towards the predicted one, after a correction.
    /// @details Call it once a frame: the gap a reconciliation left (see drawn()) shrinks by
    /// smoothing_speed per second, so the ship never jumps by more than its own move plus
    /// 2 pixels a frame.
    /// @param seconds Time since the last call.
    void smooth(float seconds) noexcept;

    /// @brief Where to draw the ship: position(), plus what is left of the corrections.
    /// @details A reconciliation moves position() at once, but the drawn ship stays where it
    /// was and smooth() brings it there, unless the correction is snap_distance or more.
    /// start() and stop() leave no gap.
    /// @return The center to draw, or nothing before start() or after stop().
    [[nodiscard]] std::optional<aegis2d::Vec2> drawn() const noexcept;

    /// @brief Network id of the ship predicted.
    /// @return The id, or nothing before start() or after stop().
    [[nodiscard]] std::optional<std::uint32_t> ship() const noexcept;

    /// @brief Where the ship is, its inputs applied.
    /// @return The predicted center, or nothing before start() or after stop().
    [[nodiscard]] std::optional<aegis2d::Vec2> position() const noexcept;

    /// @brief The inputs applied since start() that the server has not acknowledged.
    /// @return Oldest first.
    [[nodiscard]] const std::deque<InputRecord>& pending() const noexcept;

private:
    std::optional<std::uint32_t> ship_;
    aegis2d::Vec2 position_;
    std::deque<InputRecord> pending_;
    std::uint64_t corrections_ = 0;
    float last_correction_ = 0.0F;
    // drawn() minus position_: the part of the corrections not shown yet.
    aegis2d::Vec2 gap_;
};

} // namespace rtype
