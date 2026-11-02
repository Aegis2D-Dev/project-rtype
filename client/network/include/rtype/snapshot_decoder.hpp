/// @file
/// @brief The client side of delta snapshots: each snapshot rebuilt from its baseline.
#pragma once
#include <rtype/delta_snapshot.hpp>
#include <rtype/snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace rtype {

/// @brief Rebuilds the snapshots of the server from the payloads of its `Snapshot` datagrams.
/// @details Each payload is encoded against a baseline the client acknowledged, or none
/// (write_delta_snapshot()). The decoder keeps the last `capacity` snapshots it rebuilt, as
/// the baselines of the next ones, and the payloads of each one, as a snapshot may come in
/// several datagrams. A payload whose baseline it no longer has, or that does not match it,
/// is dropped: the datagram that carried it must not be acknowledged, so that the server
/// never picks that snapshot as a baseline.
class SnapshotDecoder {
public:
    /// @brief Snapshots kept, as many as the server keeps per client.
    static constexpr std::size_t capacity = 32;

    /// @brief Tells whether a payload can be decoded: a full snapshot, or a delta whose
    /// baseline is kept.
    /// @param part A payload read by read_delta_snapshot().
    /// @return `true` if decode() can rebuild its snapshot.
    [[nodiscard]] bool decodable(const DeltaSnapshot& part) const noexcept;

    /// @brief Rebuilds the snapshot of a payload, with the payloads of its tick received so
    /// far.
    /// @details The snapshot is kept, replacing the one of the same tick, and the oldest
    /// beyond capacity is forgotten. Call it once per datagram: a duplicate would count
    /// twice.
    /// @param part A payload read by read_delta_snapshot().
    /// @return The snapshot, or `std::nullopt` if the payload is dropped (counted by
    /// dropped()).
    [[nodiscard]] std::optional<Snapshot> decode(const DeltaSnapshot& part);

    /// @brief A snapshot kept.
    /// @param tick Its tick.
    /// @return The snapshot, or `nullptr` if none of that tick is kept.
    [[nodiscard]] const Snapshot* find(std::uint32_t tick) const noexcept;
    /// @brief Number of snapshots kept.
    /// @return From 0 to capacity.
    [[nodiscard]] std::size_t size() const noexcept;
    /// @brief Number of payloads dropped by decode().
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t dropped() const noexcept;

private:
    struct Rebuilt {
        std::optional<std::uint32_t> baseline;
        std::vector<DeltaSnapshot> parts;
        Snapshot snapshot;
    };
    [[nodiscard]] static std::uint32_t tick_of(const Rebuilt& kept) noexcept;
    [[nodiscard]] const Rebuilt* entry(std::uint32_t tick) const noexcept;
    [[nodiscard]] const Snapshot* baseline_of(const DeltaSnapshot& part) const noexcept;
    std::optional<Snapshot> drop() noexcept;

    std::deque<Rebuilt> kept_;
    std::uint64_t dropped_ = 0;
};

} // namespace rtype
