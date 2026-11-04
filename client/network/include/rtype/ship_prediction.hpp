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
};

} // namespace rtype
