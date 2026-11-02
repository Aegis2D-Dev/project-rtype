#include <rtype/delta_snapshot.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/snapshot_decoder.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace rtype {

std::uint32_t SnapshotDecoder::tick_of(const Rebuilt& kept) noexcept {
    return kept.snapshot.tick;
}

const SnapshotDecoder::Rebuilt* SnapshotDecoder::entry(std::uint32_t tick) const noexcept {
    const auto found = std::ranges::find(kept_, tick, tick_of);
    return found == kept_.end() ? nullptr : &*found;
}

const Snapshot* SnapshotDecoder::baseline_of(const DeltaSnapshot& part) const noexcept {
    return part.baseline.has_value() ? find(part.baseline.value_or(0)) : nullptr;
}

bool SnapshotDecoder::decodable(const DeltaSnapshot& part) const noexcept {
    return !part.baseline.has_value() || baseline_of(part) != nullptr;
}

std::optional<Snapshot> SnapshotDecoder::drop() noexcept {
    ++dropped_;
    return std::nullopt;
}

std::optional<Snapshot> SnapshotDecoder::decode(const DeltaSnapshot& part) {
    if (!decodable(part)) {
        return drop();
    }
    // The other payloads of its tick, received before it, against the same baseline.
    const auto known = std::ranges::find(kept_, part.tick, tick_of);
    std::vector<DeltaSnapshot> parts;
    if (known != kept_.end()) {
        if (known->baseline != part.baseline) {
            return drop();
        }
        parts = known->parts;
    }
    parts.push_back(part);
    std::optional<Snapshot> snapshot = apply_delta(baseline_of(part), parts);
    if (!snapshot) {
        return drop();
    }
    Rebuilt rebuilt{.baseline = part.baseline,
                    .parts = std::move(parts),
                    .snapshot = snapshot.value_or(Snapshot{})};
    if (known != kept_.end()) {
        *known = std::move(rebuilt);
        return snapshot;
    }
    kept_.push_back(std::move(rebuilt));
    if (kept_.size() > capacity) {
        kept_.erase(std::ranges::min_element(kept_, {}, tick_of));
    }
    return snapshot;
}

const Snapshot* SnapshotDecoder::find(std::uint32_t tick) const noexcept {
    const Rebuilt* const found = entry(tick);
    return found == nullptr ? nullptr : &found->snapshot;
}

std::size_t SnapshotDecoder::size() const noexcept {
    return kept_.size();
}

std::uint64_t SnapshotDecoder::dropped() const noexcept {
    return dropped_;
}

} // namespace rtype
