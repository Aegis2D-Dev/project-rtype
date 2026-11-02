#include <catch2/catch_test_macros.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/snapshot_history.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

using rtype::Snapshot;
using rtype::SnapshotHistory;

namespace {

std::shared_ptr<const Snapshot> snapshot(std::uint32_t tick) {
    return std::make_shared<const Snapshot>(Snapshot{.tick = tick, .players = {}, .entities = {}});
}

void send(SnapshotHistory& history, std::uint32_t tick,
          std::initializer_list<std::uint16_t> sequences) {
    const std::vector<std::uint16_t> list{sequences};
    history.sent(snapshot(tick), list);
}

std::uint32_t latest(const SnapshotHistory& history) {
    const Snapshot* const acknowledged = history.latest_acknowledged();
    return acknowledged == nullptr ? 0 : acknowledged->tick;
}

} // namespace

TEST_CASE("nothing is acknowledged before the client says so", "[server][baselines]") {
    SnapshotHistory history;
    CHECK(history.latest_acknowledged() == nullptr);
    send(history, 3, {10});
    CHECK(history.size() == 1);
    CHECK(history.find(3) != nullptr);
    CHECK(history.find(6) == nullptr);
    CHECK(!history.acknowledged(3));
    CHECK(history.latest_acknowledged() == nullptr);
    // An acknowledgement of other datagrams (an Accept, an Event) changes nothing.
    history.acknowledge(9, 0xFFFFFFFFU);
    CHECK(history.latest_acknowledged() == nullptr);
}

TEST_CASE("the latest snapshot acknowledged is the one of the highest tick",
          "[server][baselines]") {
    SnapshotHistory history;
    send(history, 3, {10});
    send(history, 6, {12});
    send(history, 9, {13});

    history.acknowledge(10, 0);
    CHECK(latest(history) == 3);
    // ack 13 with bit 0 (12): 9 and 6; ack_bits names 12 = 13 - 0 - 1.
    history.acknowledge(13, 0b1U);
    CHECK(latest(history) == 9);
    CHECK(history.acknowledged(6));
    // A late datagram acknowledging an older one does not move it back.
    history.acknowledge(10, 0);
    CHECK(latest(history) == 9);
}

TEST_CASE("a snapshot in several datagrams is acknowledged once all are", "[server][baselines]") {
    SnapshotHistory history;
    send(history, 3, {20, 21, 22});
    history.acknowledge(20, 0);
    history.acknowledge(22, 0);
    CHECK(!history.acknowledged(3));
    CHECK(history.latest_acknowledged() == nullptr);
    // The missing one, through ack_bits of a later datagram: 23 - 1 - 1 = 21.
    history.acknowledge(23, 0b10U);
    CHECK(history.acknowledged(3));
    CHECK(latest(history) == 3);
}

TEST_CASE("acknowledgements reach 32 datagrams back and across the sequence wrap",
          "[server][baselines]") {
    SnapshotHistory history;
    send(history, 3, {65530});
    send(history, 6, {65535});
    send(history, 9, {2});
    // ack 2: 65535 is 3 back (bit 2), 65530 is 8 back (bit 7).
    history.acknowledge(2, 1U << 2U);
    CHECK(latest(history) == 9);
    CHECK(history.acknowledged(6));
    CHECK(!history.acknowledged(3));
    history.acknowledge(2, 1U << 7U);
    CHECK(history.acknowledged(3));

    // 33 back is out of ack_bits.
    SnapshotHistory far;
    send(far, 3, {100});
    far.acknowledge(133, 0xFFFFFFFFU);
    CHECK(!far.acknowledged(3));
    far.acknowledge(132, 1U << 31U);
    CHECK(far.acknowledged(3));
}

TEST_CASE("only the last 32 snapshots are kept", "[server][baselines]") {
    SnapshotHistory history;
    send(history, 1, {0});
    history.acknowledge(0, 0);
    CHECK(latest(history) == 1);
    for (std::uint16_t index = 1; index <= SnapshotHistory::capacity; ++index) {
        send(history, 1U + (3U * index), {index});
        CHECK(history.size() == std::min<std::size_t>(index + 1, SnapshotHistory::capacity));
    }
    // The snapshot of tick 1 is gone, and with it the only one acknowledged.
    CHECK(history.find(1) == nullptr);
    CHECK(history.latest_acknowledged() == nullptr);
    CHECK(history.find(4) != nullptr);
    history.acknowledge(SnapshotHistory::capacity, 0);
    CHECK(latest(history) == 1 + (3 * SnapshotHistory::capacity));
}
