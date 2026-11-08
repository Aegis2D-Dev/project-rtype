#include <aegis2d/net/rtt.hpp>
#include <aegis2d/net/traffic.hpp>

#include <rtype/lagometer.hpp>
#include <rtype/network_stats.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

namespace {

// Per graph: the lowest top of its scale, then the values from which it is a Warning and
// Bad. Snapshots come every 50 ms: 60 ms means some are late, 100 ms that one in two is.
struct GraphLimits {
    double floor;
    double warning;
    double bad;
};

constexpr std::array<GraphLimits, lag_graph_count> limits{{
    {.floor = 100.0, .warning = 100.0, .bad = 200.0}, // RTT, ms
    {.floor = 20.0, .warning = 10.0, .bad = 30.0},    // jitter, ms
    {.floor = 10.0, .warning = 1.0, .bad = 5.0},      // loss, %
    {.floor = 100.0, .warning = 60.0, .bad = 100.0},  // snapshot interval, ms
    {.floor = 4096.0, .warning = 0.0, .bad = 0.0},    // bytes in per second
    {.floor = 4096.0, .warning = 0.0, .bad = 0.0},    // bytes out per second
    {.floor = 10.0, .warning = 2.0, .bad = 10.0},     // corrections per second
}};

const GraphLimits& limits_of(LagGraph graph) noexcept {
    return limits.at(static_cast<std::size_t>(graph));
}

double milliseconds(std::chrono::steady_clock::duration duration) noexcept {
    return std::chrono::duration<double, std::milli>{duration}.count();
}

} // namespace

LagSample measure_lag(const net::RttEstimator& rtt, const net::TrafficCounters& traffic,
                      const NetworkStats& stats, std::chrono::steady_clock::time_point now) {
    const NetworkSummary summary = stats.summary(now);
    LagSample sample;
    sample.values = {
        milliseconds(rtt.smoothed()),
        milliseconds(summary.snapshot_jitter),
        summary.loss * 100.0,
        milliseconds(summary.snapshot_interval),
        static_cast<double>(traffic.last_second(net::Direction::Received, now).bytes),
        static_cast<double>(traffic.last_second(net::Direction::Sent, now).bytes),
        0.0,
    };
    return sample;
}

LagLevel lag_level(LagGraph graph, double value) noexcept {
    const GraphLimits& limit = limits_of(graph);
    if (limit.bad <= 0.0) {
        return LagLevel::Good;
    }
    if (value >= limit.bad) {
        return LagLevel::Bad;
    }
    return value >= limit.warning ? LagLevel::Warning : LagLevel::Good;
}

std::string lag_label(LagGraph graph, double value) {
    switch (graph) {
    case LagGraph::Rtt:
        return std::format("RTT {:.0f} MS", value);
    case LagGraph::Jitter:
        return std::format("JITTER {:.0f} MS", value);
    case LagGraph::Loss:
        return std::format("LOSS {:.1f} %", value);
    case LagGraph::Interval:
        return std::format("SNAPSHOTS {:.0f} MS", value);
    case LagGraph::BytesIn:
        return std::format("IN {:.1f} KB/S", value / 1024.0);
    case LagGraph::BytesOut:
        return std::format("OUT {:.1f} KB/S", value / 1024.0);
    case LagGraph::Corrections:
        return std::format("CORRECTED {:.0f}/S", value);
    }
    return {};
}

std::vector<std::string> lag_lines(const LagSample& sample) {
    const auto label = [&sample](LagGraph graph) { return lag_label(graph, sample.value(graph)); };
    return {std::format("{}  {}  {}", label(LagGraph::Rtt), label(LagGraph::Jitter),
                        label(LagGraph::Loss)),
            std::format("{}  {}", label(LagGraph::BytesIn), label(LagGraph::BytesOut)),
            std::format("{}  {}", label(LagGraph::Interval), label(LagGraph::Corrections))};
}

void Lagometer::update(const net::RttEstimator& rtt, const net::TrafficCounters& traffic,
                       const NetworkStats& stats, Clock::time_point now,
                       std::uint64_t corrections) {
    if (now < next_) {
        return;
    }
    LagSample sample = measure_lag(rtt, traffic, stats, now);
    // Those since the last sample, per second; none for the first sample of a session.
    if (const std::optional<Clock::time_point> last = sampled_; last && now > *last) {
        const double seconds = std::chrono::duration<double>{now - *last}.count();
        sample.values.at(static_cast<std::size_t>(LagGraph::Corrections)) =
            static_cast<double>(corrections - std::min(corrections, corrections_)) / seconds;
    }
    corrections_ = corrections;
    sampled_ = now;
    add(sample);
    // After a stall, the next sample comes an interval later, without a burst.
    next_ = now - next_ < sample_interval ? next_ + sample_interval : now + sample_interval;
}

void Lagometer::add(const LagSample& sample) {
    samples_.push_back(sample);
    while (samples_.size() > length) {
        samples_.pop_front();
    }
}

void Lagometer::clear() noexcept {
    samples_.clear();
    next_ = {};
    corrections_ = 0;
    sampled_.reset();
}

const std::deque<LagSample>& Lagometer::samples() const noexcept {
    return samples_;
}

LagSample Lagometer::latest() const noexcept {
    return samples_.empty() ? LagSample{} : samples_.back();
}

double Lagometer::scale(LagGraph graph) const noexcept {
    double top = limits_of(graph).floor;
    for (const LagSample& sample : samples_) {
        top = std::max(top, sample.value(graph));
    }
    return top;
}

} // namespace rtype
