/// @file
/// @brief The figures of the lagometer: the network statistics of the last seconds, sampled.
#pragma once
#include <aegis2d/net/rtt.hpp>
#include <aegis2d/net/traffic.hpp>

#include <rtype/network_stats.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace rtype {

/// @brief A graph of the lagometer.
enum class LagGraph : std::uint8_t {
    /// @brief Smoothed round trip time, in milliseconds.
    Rtt,
    /// @brief Jitter of the snapshots' arrivals, in milliseconds.
    Jitter,
    /// @brief Share of the server's datagrams lost, in percent.
    Loss,
    /// @brief Mean time between two snapshots, in milliseconds.
    Interval,
    /// @brief Bytes received per second.
    BytesIn,
    /// @brief Bytes sent per second.
    BytesOut,
    /// @brief Corrections of the predicted ship per second (ShipPrediction::corrections()).
    Corrections,
};

/// @brief Number of graphs: LagGraph values are 0 to lag_graph_count - 1.
inline constexpr std::size_t lag_graph_count = 7;

/// @brief How a figure compares with a sound link.
enum class LagLevel : std::uint8_t {
    /// @brief As on a local network.
    Good,
    /// @brief Players start to notice.
    Warning,
    /// @brief The game is hard to play.
    Bad,
};

/// @brief The figures of every graph at one time, in the unit of each graph.
struct LagSample {
    /// @brief One value per graph, indexed by LagGraph.
    std::array<double, lag_graph_count> values{};

    /// @brief The value of a graph.
    /// @param graph Graph to read.
    /// @return Its value.
    [[nodiscard]] double value(LagGraph graph) const noexcept {
        return values.at(static_cast<std::size_t>(graph));
    }
};

/// @brief Reads the network statistics of the client into a sample.
/// @param rtt Round trip time to the server (ClientSession::rtt()).
/// @param traffic Datagrams exchanged (ClientSession::traffic()): the last whole second.
/// @param stats Loss and snapshots (ClientSession::network_stats()): the last 2 seconds.
/// @param now Current time.
/// @return The sample.
[[nodiscard]] LagSample measure_lag(const aegis2d::net::RttEstimator& rtt,
                                    const aegis2d::net::TrafficCounters& traffic,
                                    const NetworkStats& stats,
                                    std::chrono::steady_clock::time_point now);

/// @brief Level of a value of a graph: the RTT is bad above 200 ms, the loss above 5 %...
/// @details The bandwidth is always Good: how much is too much depends on the link.
/// @param graph Graph of the value.
/// @param value Value, in the unit of the graph.
/// @return Its level.
[[nodiscard]] LagLevel lag_level(LagGraph graph, double value) noexcept;

/// @brief Label of a value of a graph, for the overlay: `"RTT 152 MS"`, `"LOSS 2.5 %"`.
/// @param graph Graph of the value.
/// @param value Value, in the unit of the graph.
/// @return The label, in capitals for the pixel font.
[[nodiscard]] std::string lag_label(LagGraph graph, double value);

/// @brief The figures of a sample as lines of the engine's metrics overlay, so that the
/// frame metrics (F12) show the network too, with or without the lagometer (F6).
/// @param sample Figures, such as Lagometer::latest().
/// @return Three lines: `"RTT 42 MS  JITTER 3 MS  LOSS 0.0 %"`, `"IN 1.5 KB/S  OUT 0.8 KB/S"`
/// and `"SNAPSHOTS 50 MS  CORRECTED 0/S"`.
[[nodiscard]] std::vector<std::string> lag_lines(const LagSample& sample);

/// @brief The last seconds of the network statistics, for the graphs of the lagometer.
/// @details update() takes a sample every sample_interval and keeps the last length ones,
/// oldest first: a graph draws one bar per sample, on the scale() of its graph.
class Lagometer {
public:
    /// @brief Clock of the samples.
    using Clock = std::chrono::steady_clock;
    /// @brief Time between two samples.
    static constexpr std::chrono::milliseconds sample_interval{100};
    /// @brief Samples kept: the last 10 seconds.
    static constexpr std::size_t length = 100;

    /// @brief Takes a sample of the statistics if sample_interval went by since the last one.
    /// @param rtt Round trip time to the server.
    /// @param traffic Datagrams exchanged with the server.
    /// @param stats Loss and snapshots of the server's datagrams.
    /// @param now Current time.
    /// @param corrections Corrections of the predicted ship so far
    /// (ShipPrediction::corrections()): the sample gives those since the last one, per second.
    void update(const aegis2d::net::RttEstimator& rtt, const aegis2d::net::TrafficCounters& traffic,
                const NetworkStats& stats, Clock::time_point now, std::uint64_t corrections = 0);

    /// @brief Adds a sample, forgetting the oldest beyond length.
    /// @param sample Sample to add.
    void add(const LagSample& sample);

    /// @brief Forgets every sample, for a new session.
    void clear() noexcept;

    /// @brief The samples kept.
    /// @return At most length samples, oldest first.
    [[nodiscard]] const std::deque<LagSample>& samples() const noexcept;

    /// @brief The last sample.
    /// @return It, or a sample of zeros before the first one.
    [[nodiscard]] LagSample latest() const noexcept;

    /// @brief Top of a graph: its largest value kept, but never below a floor (100 ms for
    /// the RTT, 10 % for the loss...), so that a sound link draws low bars.
    /// @param graph Graph to scale.
    /// @return The value drawn at the full height of the graph.
    [[nodiscard]] double scale(LagGraph graph) const noexcept;

private:
    std::deque<LagSample> samples_;
    Clock::time_point next_{};
    // Corrections and time of the last sample, for the corrections per second.
    std::uint64_t corrections_ = 0;
    std::optional<Clock::time_point> sampled_;
};

} // namespace rtype
