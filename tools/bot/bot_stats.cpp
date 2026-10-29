#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/traffic.hpp>

#include <rtype/bot.hpp>
#include <rtype/bot_stats.hpp>
#include <rtype/client_session.hpp>
#include <rtype/network_stats.hpp>

#include <chrono>
#include <format>
#include <string>
#include <utility>

namespace rtype {

namespace net = aegis2d::net;

namespace {

double milliseconds(std::chrono::steady_clock::duration duration) {
    return std::chrono::duration<double, std::milli>{duration}.count();
}

} // namespace

BotStats measure_bot(const Bot& bot, std::string name, std::chrono::steady_clock::duration played,
                     std::chrono::steady_clock::time_point now) {
    const ClientSession& session = bot.session();
    const net::TrafficCounters& traffic = session.traffic();
    const NetworkSummary summary = session.network_stats().summary(now);
    const double seconds = std::chrono::duration<double>{played}.count();
    const auto per_second = [seconds](std::uint64_t bytes) {
        return seconds > 0.0 ? static_cast<double>(bytes) / seconds : 0.0;
    };
    return BotStats{
        .name = std::move(name),
        .seconds = seconds,
        .inputs_sent = session.inputs_sent(),
        .snapshots = traffic.total(net::Direction::Received, net::PacketType::Snapshot).datagrams,
        .bytes_in_per_second = per_second(traffic.total(net::Direction::Received).bytes),
        .bytes_out_per_second = per_second(traffic.total(net::Direction::Sent).bytes),
        .rtt_ms = milliseconds(session.rtt().smoothed()),
        .rtt_jitter_ms = milliseconds(session.rtt().jitter()),
        .loss_percent = summary.loss * 100.0,
        .snapshot_jitter_ms = milliseconds(summary.snapshot_jitter),
    };
}

std::string bot_stats_line(const BotStats& stats) {
    return std::format("{},{:.3f},{},{},{:.1f},{:.1f},{:.2f},{:.2f},{:.2f},{:.2f}", stats.name,
                       stats.seconds, stats.inputs_sent, stats.snapshots, stats.bytes_in_per_second,
                       stats.bytes_out_per_second, stats.rtt_ms, stats.rtt_jitter_ms,
                       stats.loss_percent, stats.snapshot_jitter_ms);
}

} // namespace rtype
