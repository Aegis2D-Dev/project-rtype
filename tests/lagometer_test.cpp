#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/rtt.hpp>
#include <aegis2d/net/traffic.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <rtype/lagometer.hpp>
#include <rtype/network_stats.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>

using aegis2d::net::Direction;
using aegis2d::net::PacketType;
using aegis2d::net::RttEstimator;
using aegis2d::net::TrafficCounters;
using rtype::LagGraph;
using rtype::LagLevel;
using rtype::Lagometer;
using rtype::LagSample;
using rtype::NetworkStats;
using std::chrono::milliseconds;
using Clock = Lagometer::Clock;

namespace {

constexpr Clock::time_point start = Clock::time_point{} + std::chrono::hours{1};

// A link to the server as the client sees it, 20 snapshots a second.
struct Link {
    RttEstimator rtt;
    TrafficCounters traffic;
    NetworkStats stats;
    Lagometer lagometer;
    std::uint16_t sequence = 0;
    Clock::time_point now = start;

    // Runs the link for @p seconds: each snapshot takes @p rtt to come back, one in
    // @p lose_every is lost (none with 0), and every other one arrives @p jitter late.
    void run(int seconds, milliseconds rtt_time, int lose_every, milliseconds jitter) {
        for (int step = 0; step < seconds * 20; ++step) {
            now += milliseconds{50};
            ++sequence;
            if (lose_every == 0 || sequence % lose_every != 0) {
                const Clock::time_point arrival =
                    now + (sequence % 2 == 0 ? jitter : milliseconds{});
                rtt.add_sample(rtt_time);
                traffic.record(Direction::Received, PacketType::Snapshot, 120, arrival);
                stats.record(sequence, PacketType::Snapshot, arrival);
            }
            traffic.record(Direction::Sent, PacketType::Input, 24, now);
            // Read after the late snapshots arrived.
            lagometer.update(rtt, traffic, stats, now + milliseconds{20});
        }
    }

    [[nodiscard]] LagLevel level(LagGraph graph) const {
        return rtype::lag_level(graph, lagometer.latest().value(graph));
    }
};

} // namespace

TEST_CASE("the lagometer samples ten times a second and keeps ten seconds", "[client][lag]") {
    const RttEstimator rtt;
    const TrafficCounters traffic;
    const NetworkStats stats;
    Lagometer lagometer;
    CHECK(lagometer.samples().empty());
    CHECK(lagometer.latest().value(LagGraph::Rtt) == 0.0);
    // Frames at 60 per second for 2 seconds: 20 samples.
    for (int frame = 0; frame < 120; ++frame) {
        lagometer.update(rtt, traffic, stats, start + (frame * milliseconds{1000}) / 60);
    }
    CHECK(lagometer.samples().size() == 20);
    // A stall of a second gives one sample, not ten.
    lagometer.update(rtt, traffic, stats, start + milliseconds{3000});
    lagometer.update(rtt, traffic, stats, start + milliseconds{3010});
    CHECK(lagometer.samples().size() == 21);
    for (std::size_t index = 0; index < 2 * Lagometer::length; ++index) {
        lagometer.add(LagSample{});
    }
    CHECK(lagometer.samples().size() == Lagometer::length);
    lagometer.clear();
    CHECK(lagometer.samples().empty());
}

TEST_CASE("a sample reads every statistic in the unit of its graph", "[client][lag]") {
    RttEstimator rtt;
    rtt.add_sample(milliseconds{80});
    TrafficCounters traffic;
    NetworkStats stats;
    // Two seconds of snapshots of 512 bytes, 1 in 4 lost, and inputs of 64 bytes.
    for (int index = 0; index < 40; ++index) {
        const Clock::time_point at = start + milliseconds{50 * index};
        if (index % 4 != 1) {
            traffic.record(Direction::Received, PacketType::Snapshot, 512, at);
            stats.record(static_cast<std::uint16_t>(index), PacketType::Snapshot, at);
        }
        traffic.record(Direction::Sent, PacketType::Input, 64, at);
    }
    const LagSample sample = rtype::measure_lag(rtt, traffic, stats, start + milliseconds{1950});
    CHECK(sample.value(LagGraph::Rtt) == Catch::Approx(80.0));
    CHECK(sample.value(LagGraph::Loss) == Catch::Approx(25.0));
    CHECK(sample.value(LagGraph::Interval) > 60.0);
    CHECK(sample.value(LagGraph::Jitter) > 20.0);
    // The last whole second, the first: 15 snapshots received and 20 inputs sent.
    CHECK(sample.value(LagGraph::BytesIn) == Catch::Approx(15.0 * 512.0));
    CHECK(sample.value(LagGraph::BytesOut) == Catch::Approx(20.0 * 64.0));
}

TEST_CASE("each graph has its levels and its label", "[client][lag]") {
    CHECK(rtype::lag_level(LagGraph::Rtt, 30.0) == LagLevel::Good);
    CHECK(rtype::lag_level(LagGraph::Rtt, 150.0) == LagLevel::Warning);
    CHECK(rtype::lag_level(LagGraph::Rtt, 300.0) == LagLevel::Bad);
    CHECK(rtype::lag_level(LagGraph::Loss, 0.0) == LagLevel::Good);
    CHECK(rtype::lag_level(LagGraph::Loss, 2.0) == LagLevel::Warning);
    CHECK(rtype::lag_level(LagGraph::Loss, 5.0) == LagLevel::Bad);
    CHECK(rtype::lag_level(LagGraph::Interval, 50.0) == LagLevel::Good);
    CHECK(rtype::lag_level(LagGraph::Interval, 100.0) == LagLevel::Bad);
    CHECK(rtype::lag_level(LagGraph::BytesIn, 1.0e9) == LagLevel::Good);
    CHECK(rtype::lag_label(LagGraph::Rtt, 152.4) == "RTT 152 MS");
    CHECK(rtype::lag_label(LagGraph::Loss, 2.5) == "LOSS 2.5 %");
    CHECK(rtype::lag_label(LagGraph::Interval, 50.0) == "SNAPSHOTS 50 MS");
    CHECK(rtype::lag_label(LagGraph::BytesIn, 2560.0) == "IN 2.5 KB/S");
    CHECK(rtype::lag_label(LagGraph::BytesOut, 1024.0) == "OUT 1.0 KB/S");
}

TEST_CASE("the lagometer counts the corrections of the prediction per second", "[client][lag]") {
    const RttEstimator rtt;
    const TrafficCounters traffic;
    const NetworkStats stats;
    Lagometer lagometer;
    // The first sample has nothing to compare with; then 3 corrections in 100 ms, none in the
    // next 200 ms.
    lagometer.update(rtt, traffic, stats, start, 7);
    CHECK(lagometer.latest().value(LagGraph::Corrections) == 0.0);
    lagometer.update(rtt, traffic, stats, start + milliseconds{100}, 10);
    CHECK(lagometer.latest().value(LagGraph::Corrections) == Catch::Approx(30.0));
    lagometer.update(rtt, traffic, stats, start + milliseconds{300}, 10);
    CHECK(lagometer.latest().value(LagGraph::Corrections) == 0.0);
    CHECK(lagometer.samples().size() == 3);
    CHECK(lagometer.scale(LagGraph::Corrections) == 30.0);

    CHECK(rtype::lag_label(LagGraph::Corrections, 3.0) == "CORRECTED 3/S");
    CHECK(rtype::lag_level(LagGraph::Corrections, 0.0) == LagLevel::Good);
    CHECK(rtype::lag_level(LagGraph::Corrections, 3.0) == LagLevel::Warning);
    CHECK(rtype::lag_level(LagGraph::Corrections, 12.0) == LagLevel::Bad);

    // A new session starts from zero.
    lagometer.clear();
    lagometer.update(rtt, traffic, stats, start + milliseconds{400}, 0);
    CHECK(lagometer.latest().value(LagGraph::Corrections) == 0.0);
}

TEST_CASE("the scale of a graph follows its largest value above a floor", "[client][lag]") {
    Lagometer lagometer;
    CHECK(lagometer.scale(LagGraph::Rtt) == 100.0);
    LagSample sample;
    sample.values.at(static_cast<std::size_t>(LagGraph::Rtt)) = 40.0;
    lagometer.add(sample);
    CHECK(lagometer.scale(LagGraph::Rtt) == 100.0);
    sample.values.at(static_cast<std::size_t>(LagGraph::Rtt)) = 350.0;
    lagometer.add(sample);
    CHECK(lagometer.scale(LagGraph::Rtt) == 350.0);
}

TEST_CASE("the lagometer shows a degraded link, then its return to normal", "[client][lag]") {
    // As netem would: 20 ms on a local network, then 150 ms more, 10 ms of jitter and 1
    // datagram in 10 lost for 5 seconds, then the local network again.
    Link link;
    link.run(5, milliseconds{20}, 0, milliseconds{0});
    for (const LagGraph graph :
         {LagGraph::Rtt, LagGraph::Jitter, LagGraph::Loss, LagGraph::Interval}) {
        CHECK(link.level(graph) == LagLevel::Good);
    }
    CHECK(link.lagometer.latest().value(LagGraph::Interval) == Catch::Approx(50.0));
    CHECK(link.lagometer.latest().value(LagGraph::BytesIn) == Catch::Approx(20.0 * 120.0));

    link.run(5, milliseconds{170}, 10, milliseconds{10});
    CHECK(link.level(LagGraph::Rtt) == LagLevel::Warning);
    CHECK(link.lagometer.latest().value(LagGraph::Rtt) > 165.0);
    CHECK(link.level(LagGraph::Jitter) != LagLevel::Good);
    CHECK(link.level(LagGraph::Loss) == LagLevel::Bad);
    CHECK(link.lagometer.latest().value(LagGraph::BytesIn) < 20.0 * 120.0);
    CHECK(link.lagometer.scale(LagGraph::Rtt) > 165.0);

    link.run(5, milliseconds{20}, 0, milliseconds{0});
    for (const LagGraph graph :
         {LagGraph::Rtt, LagGraph::Jitter, LagGraph::Loss, LagGraph::Interval}) {
        CHECK(link.level(graph) == LagLevel::Good);
    }
    CHECK(link.lagometer.latest().value(LagGraph::Rtt) < 25.0);
    CHECK(link.lagometer.latest().value(LagGraph::Loss) == 0.0);
    // The graphs still show the degraded seconds, on the left.
    CHECK(link.lagometer.samples().size() == Lagometer::length);
    CHECK(link.lagometer.scale(LagGraph::Rtt) > 150.0);
}
