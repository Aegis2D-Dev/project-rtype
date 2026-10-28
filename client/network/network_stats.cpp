#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/sequence.hpp>

#include <rtype/network_stats.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

void NetworkStats::record(std::uint16_t sequence, net::PacketType type, Clock::time_point arrival) {
    arrivals_.push_back(Arrival{
        .sequence = sequence, .snapshot = type == net::PacketType::Snapshot, .time = arrival});
    while (arrivals_.front().time <= arrival - window) {
        arrivals_.pop_front();
    }
}

NetworkSummary NetworkStats::summary(Clock::time_point now) const {
    NetworkSummary summary;
    std::vector<const Arrival*> recent;
    for (const Arrival& arrival : arrivals_) {
        if (arrival.time > now - window && arrival.time <= now) {
            recent.push_back(&arrival);
        }
    }
    if (recent.empty()) {
        return summary;
    }
    // The newest sequence of the window, the counter having possibly wrapped.
    std::uint16_t newest = recent.front()->sequence;
    for (const Arrival* const arrival : recent) {
        if (net::sequence_greater_than(arrival->sequence, newest)) {
            newest = arrival->sequence;
        }
    }

    // Each sequence by its distance behind the newest: the distinct ones were received, the
    // others up to the farthest were lost.
    std::vector<std::uint16_t> behind;
    std::vector<Clock::time_point> snapshots;
    for (const Arrival* const arrival : recent) {
        const auto distance = static_cast<std::uint16_t>(newest - arrival->sequence);
        if (distance < net::sequence_half_range) {
            behind.push_back(distance);
        }
        if (arrival->snapshot) {
            snapshots.push_back(arrival->time);
        }
    }
    std::ranges::sort(behind);
    const auto [first_duplicate, end] = std::ranges::unique(behind);
    behind.erase(first_duplicate, end);
    const std::uint32_t span = behind.back() + 1U;
    summary.received = static_cast<std::uint32_t>(behind.size());
    summary.lost = span - summary.received;
    summary.loss = static_cast<double>(summary.lost) / static_cast<double>(span);

    summary.snapshots = static_cast<std::uint32_t>(snapshots.size());
    if (snapshots.size() >= 2) {
        const auto intervals = static_cast<Clock::rep>(snapshots.size() - 1);
        summary.snapshot_interval = (snapshots.back() - snapshots.front()) / intervals;
    }
    if (snapshots.size() >= 3) {
        Clock::duration spread{};
        for (std::size_t index = 1; index < snapshots.size(); ++index) {
            const Clock::duration interval = snapshots.at(index) - snapshots.at(index - 1);
            spread += std::chrono::abs(interval - summary.snapshot_interval);
        }
        summary.snapshot_jitter = spread / static_cast<Clock::rep>(snapshots.size() - 1);
    }
    return summary;
}

} // namespace rtype
