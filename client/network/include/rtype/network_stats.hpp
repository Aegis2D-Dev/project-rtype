/// @file
/// @brief Packet loss, snapshot interval and jitter of the datagrams the client receives.
#pragma once
#include <aegis2d/net/packet.hpp>

#include <chrono>
#include <cstdint>
#include <deque>

namespace rtype {

/// @brief What the client received from the server over the last NetworkStats::window.
struct NetworkSummary {
    /// @brief Datagrams received, each sequence counted once.
    std::uint32_t received = 0;
    /// @brief Sequences missing between the oldest and the newest received.
    std::uint32_t lost = 0;
    /// @brief Share of the datagrams lost, from 0 to 1: lost / (received + lost).
    double loss = 0.0;
    /// @brief `Snapshot` datagrams received.
    std::uint32_t snapshots = 0;
    /// @brief Mean time between two snapshots, 0 with fewer than two.
    std::chrono::steady_clock::duration snapshot_interval{};
    /// @brief Mean distance of each interval between two snapshots from their mean: how
    /// irregularly the snapshots arrive. 0 with fewer than three snapshots.
    std::chrono::steady_clock::duration snapshot_jitter{};
};

/// @brief Measures the loss and the jitter of the server's datagrams over a sliding window.
/// @details The server numbers every datagram it sends to this client, whatever its type
/// (RFC section 2.2): a sequence missing between two received ones is a lost datagram. The
/// snapshots, sent at a fixed rate, show the jitter: the spread of the times between their
/// arrivals. Next to ClientSession::traffic() and ClientSession::rtt(), the source of the
/// network statistics shown to the player.
class NetworkStats {
public:
    /// @brief Clock of the arrivals.
    using Clock = std::chrono::steady_clock;
    /// @brief How far back summary() looks: 40 snapshots at 20 per second.
    static constexpr std::chrono::seconds window{2};

    /// @brief Records a datagram received from the server.
    /// @details Call it for every datagram with a valid header and the session's token. A
    /// duplicate counts once; a datagram older than the window is forgotten.
    /// @param sequence The datagram's `sequence` field.
    /// @param type Its type: the `Snapshot` ones give the interval and the jitter.
    /// @param arrival Time it was received.
    void record(std::uint16_t sequence, aegis2d::net::PacketType type, Clock::time_point arrival);

    /// @brief Loss, snapshot interval and jitter of the datagrams received in the window.
    /// @param now Current time: the window is (now - window, now].
    /// @return The summary, all 0 before anything is received.
    [[nodiscard]] NetworkSummary summary(Clock::time_point now) const;

private:
    struct Arrival {
        std::uint16_t sequence = 0;
        bool snapshot = false;
        Clock::time_point time;
    };

    // Oldest first, as they arrived.
    std::deque<Arrival> arrivals_;
};

} // namespace rtype
