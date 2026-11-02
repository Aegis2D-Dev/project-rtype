/// @file
/// @brief The snapshots sent to one client and which of them it acknowledged: the baselines
/// of delta snapshots.
#pragma once
#include <rtype/snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <vector>

namespace rtype {

/// @brief The last snapshots sent to one client, and the ones it acknowledged.
/// @details A client acknowledges a snapshot by the `ack` and `ack_bits` fields of the
/// datagrams it sends (RFC section 5.1): a snapshot is acknowledged once every datagram that
/// carried it is. Only the last `capacity` snapshots sent are kept, as possible baselines of
/// the next ones.
class SnapshotHistory {
public:
    /// @brief Snapshots kept per client.
    static constexpr std::size_t capacity = 32;

    /// @brief Records a snapshot sent to the client, forgetting the oldest beyond capacity.
    /// @param snapshot Snapshot sent, shared with the histories of the other clients.
    /// @param sequences Sequences of the datagrams that carried it.
    void sent(std::shared_ptr<const Snapshot> snapshot, std::span<const std::uint16_t> sequences);

    /// @brief Records the acknowledgements of a datagram received from the client.
    /// @param ack Its `ack` field: the latest sequence of the server it received.
    /// @param ack_bits Its `ack_bits` field: bit `n` set if it received `ack - n - 1`.
    void acknowledge(std::uint16_t ack, std::uint32_t ack_bits) noexcept;

    /// @brief The most recent snapshot the client acknowledged, among those kept.
    /// @return The snapshot of the highest tick acknowledged, or `nullptr` if none is.
    [[nodiscard]] const Snapshot* latest_acknowledged() const noexcept;
    /// @brief A snapshot kept, acknowledged or not.
    /// @param tick Its tick.
    /// @return The snapshot, or `nullptr` if none of that tick is kept.
    [[nodiscard]] const Snapshot* find(std::uint32_t tick) const noexcept;
    /// @brief Tells whether the client acknowledged a snapshot.
    /// @param tick Its tick.
    /// @return `true` if a snapshot of that tick is kept and acknowledged.
    [[nodiscard]] bool acknowledged(std::uint32_t tick) const noexcept;
    /// @brief Number of snapshots kept.
    /// @return From 0 to capacity.
    [[nodiscard]] std::size_t size() const noexcept;

private:
    struct Sent {
        std::shared_ptr<const Snapshot> snapshot;
        // The sequences of its datagrams not acknowledged yet.
        std::vector<std::uint16_t> waiting;
    };
    [[nodiscard]] const Sent* entry(std::uint32_t tick) const noexcept;

    std::deque<Sent> sent_;
};

} // namespace rtype
