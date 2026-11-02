#include <rtype/snapshot.hpp>
#include <rtype/snapshot_history.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rtype {

namespace {

// The bits of ack_bits: the 32 sequences before ack.
constexpr std::uint16_t ack_window = 32;

// RFC section 5.1: ack itself, and ack - n - 1 for each bit n of ack_bits.
bool acknowledges(std::uint16_t ack, std::uint32_t ack_bits, std::uint16_t sequence) noexcept {
    const auto distance = static_cast<std::uint16_t>(ack - sequence);
    if (distance == 0U) {
        return true;
    }
    return distance <= ack_window && (ack_bits & (1U << (distance - 1U))) != 0U;
}

} // namespace

void SnapshotHistory::sent(std::shared_ptr<const Snapshot> snapshot,
                           std::span<const std::uint16_t> sequences) {
    sent_.push_back(
        Sent{.snapshot = std::move(snapshot), .waiting = {sequences.begin(), sequences.end()}});
    while (sent_.size() > capacity) {
        sent_.pop_front();
    }
}

void SnapshotHistory::acknowledge(std::uint16_t ack, std::uint32_t ack_bits) noexcept {
    for (Sent& snapshot : sent_) {
        std::erase_if(snapshot.waiting, [&](std::uint16_t sequence) {
            return acknowledges(ack, ack_bits, sequence);
        });
    }
}

const Snapshot* SnapshotHistory::latest_acknowledged() const noexcept {
    const Snapshot* latest = nullptr;
    for (const Sent& snapshot : sent_) {
        if (snapshot.waiting.empty() &&
            (latest == nullptr || snapshot.snapshot->tick > latest->tick)) {
            latest = snapshot.snapshot.get();
        }
    }
    return latest;
}

const Snapshot* SnapshotHistory::find(std::uint32_t tick) const noexcept {
    const Sent* const snapshot = entry(tick);
    return snapshot == nullptr ? nullptr : snapshot->snapshot.get();
}

bool SnapshotHistory::acknowledged(std::uint32_t tick) const noexcept {
    const Sent* const snapshot = entry(tick);
    return snapshot != nullptr && snapshot->waiting.empty();
}

std::size_t SnapshotHistory::size() const noexcept {
    return sent_.size();
}

const SnapshotHistory::Sent* SnapshotHistory::entry(std::uint32_t tick) const noexcept {
    for (const Sent& snapshot : sent_) {
        if (snapshot.snapshot->tick == tick) {
            return &snapshot;
        }
    }
    return nullptr;
}

} // namespace rtype
