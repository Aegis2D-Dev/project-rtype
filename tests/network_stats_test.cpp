#include <aegis2d/net/binary.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/client_session.hpp>
#include <rtype/login.hpp>
#include <rtype/network_stats.hpp>
#include <rtype/snapshot.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

using aegis2d::net::Endpoint;
using aegis2d::net::NetworkContext;
using aegis2d::net::PacketHeader;
using aegis2d::net::PacketType;
using aegis2d::net::TcpConnection;
using aegis2d::net::TcpListener;
using aegis2d::net::UdpSocket;
using rtype::ClientSession;
using rtype::ClientSettings;
using rtype::ClientState;
using rtype::NetworkStats;
using rtype::NetworkSummary;
using std::chrono::milliseconds;
using Clock = NetworkStats::Clock;

namespace {

// Any time far from the clock's origin, so that the window never reaches before it.
constexpr Clock::time_point start = Clock::time_point{} + std::chrono::hours{1};

// Snapshots numbered 0 to count - 1, one every 50 ms, but those @p dropped refuses, each
// arriving @p delay(index) late.
template <typename Dropped, typename Delay>
NetworkStats snapshots(int count, Dropped dropped, Delay delay) {
    NetworkStats stats;
    for (int index = 0; index < count; ++index) {
        if (!dropped(index)) {
            stats.record(static_cast<std::uint16_t>(index), PacketType::Snapshot,
                         start + milliseconds{50 * index} + delay(index));
        }
    }
    return stats;
}

milliseconds no_delay(int /*index*/) {
    return milliseconds{0};
}

bool none(int /*index*/) {
    return false;
}

} // namespace

TEST_CASE("nothing received gives an empty summary", "[client][stats]") {
    const NetworkSummary summary = NetworkStats{}.summary(start);
    CHECK(summary.received == 0);
    CHECK(summary.lost == 0);
    CHECK(summary.loss == 0.0);
    CHECK(summary.snapshot_interval == Clock::duration{});
    CHECK(summary.snapshot_jitter == Clock::duration{});
}

TEST_CASE("a regular stream loses nothing and has no jitter", "[client][stats]") {
    const NetworkSummary summary =
        snapshots(40, none, no_delay).summary(start + milliseconds{1950});
    CHECK(summary.received == 40);
    CHECK(summary.lost == 0);
    CHECK(summary.loss == 0.0);
    CHECK(summary.snapshots == 40);
    CHECK(summary.snapshot_interval == milliseconds{50});
    CHECK(summary.snapshot_jitter == Clock::duration{});
}

TEST_CASE("dropped datagrams are the gaps in the sequences", "[client][stats]") {
    // One in four lost: 1, 5... 37.
    const NetworkSummary summary = snapshots(
                                       40, [](int index) { return index % 4 == 1; }, no_delay)
                                       .summary(start + milliseconds{1950});
    CHECK(summary.received == 30);
    CHECK(summary.lost == 10);
    CHECK(summary.loss == 0.25);
    // 30 snapshots over 1950 ms: a lost one makes a gap of two intervals.
    CHECK(summary.snapshot_interval == Clock::duration{milliseconds{1950}} / 29);
    CHECK(summary.snapshot_jitter > milliseconds{20});
}

TEST_CASE("delayed datagrams give the jitter", "[client][stats]") {
    // Every other snapshot 10 ms late: intervals of 60 and 40 ms, about 10 ms from their mean.
    const NetworkSummary summary = snapshots(40, none, [](int index) {
                                       return milliseconds{index % 2 == 1 ? 10 : 0};
                                   }).summary(start + milliseconds{1960});
    CHECK(summary.lost == 0);
    CHECK(summary.snapshot_interval == Clock::duration{milliseconds{1960}} / 39);
    CHECK(summary.snapshot_jitter > milliseconds{9});
    CHECK(summary.snapshot_jitter < milliseconds{11});
}

TEST_CASE("dropped and delayed datagrams give both", "[client][stats]") {
    const NetworkSummary summary =
        snapshots(
            40, [](int index) { return index % 10 == 4; },
            [](int index) { return milliseconds{index % 2 == 1 ? 10 : 0}; })
            .summary(start + milliseconds{1960});
    CHECK(summary.received == 36);
    CHECK(summary.lost == 4);
    CHECK(summary.loss == 0.1);
    CHECK(summary.snapshot_jitter > milliseconds{10});
}

TEST_CASE("duplicates count once and late datagrams fill their gap", "[client][stats]") {
    NetworkStats stats;
    int arrival = 0;
    for (const std::uint16_t sequence : {0, 1, 3, 2, 2, 4}) {
        stats.record(sequence, PacketType::Event, start + milliseconds{10 * arrival++});
    }
    const NetworkSummary summary = stats.summary(start + milliseconds{100});
    CHECK(summary.received == 5);
    CHECK(summary.lost == 0);
    // Events are no snapshots: no interval.
    CHECK(summary.snapshots == 0);
    CHECK(summary.snapshot_interval == Clock::duration{});
}

TEST_CASE("the loss is counted across the wrap of the sequences", "[client][stats]") {
    NetworkStats stats;
    int arrival = 0;
    for (std::uint16_t sequence = 65530; sequence != 4; ++sequence) {
        if (sequence != 65535) {
            stats.record(sequence, PacketType::Snapshot, start + milliseconds{50 * arrival});
        }
        ++arrival;
    }
    const NetworkSummary summary = stats.summary(start + milliseconds{500});
    CHECK(summary.received == 9);
    CHECK(summary.lost == 1);
}

TEST_CASE("the window forgets the datagrams older than two seconds", "[client][stats]") {
    // A second with half the datagrams lost, then two clean seconds.
    NetworkStats stats;
    const auto receive = [&](int index) {
        stats.record(static_cast<std::uint16_t>(index), PacketType::Snapshot,
                     start + milliseconds{50 * index});
    };
    for (int index = 0; index < 20; index += 2) {
        receive(index);
    }
    // 0 to 18 received one in two: 9 gaps between them.
    CHECK(stats.summary(start + milliseconds{950}).lost == 9);
    for (int index = 20; index < 60; ++index) {
        receive(index);
    }
    const NetworkSummary later = stats.summary(start + milliseconds{2950});
    CHECK(later.lost == 0);
    CHECK(later.received == 40);
    CHECK(stats.summary(start + std::chrono::seconds{10}).received == 0);
}

namespace {

// Runs the context and the client until the condition holds, for at most ten seconds.
template <typename Condition>
bool pump(NetworkContext& context, ClientSession& client, Condition condition) {
    const auto deadline = Clock::now() + std::chrono::seconds{10};
    while (!condition()) {
        if (Clock::now() > deadline) {
            return false;
        }
        context.run_for(milliseconds{10});
        client.update(Clock::now());
    }
    return true;
}

} // namespace

TEST_CASE("the client counts the datagrams a server did not send", "[client][stats]") {
    // A server that answers the Connect with an Accept (0), then snapshots 1 to 20 but
    // 5, 6 and 13, as if the network had dropped them.
    NetworkContext context;
    TcpListener listener{context, 0};
    UdpSocket server_udp{context, 0};
    std::vector<std::unique_ptr<TcpConnection>> connections;
    constexpr std::uint32_t token = 77;
    const auto send = [&](const Endpoint& to, PacketType type, std::uint16_t sequence) {
        aegis2d::net::BinaryWriter writer;
        aegis2d::net::write_header(
            writer,
            PacketHeader{
                .type = type, .sequence = sequence, .ack = 0, .ack_bits = 0, .session = token});
        if (type == PacketType::Snapshot) {
            writer.write_bytes(rtype::write_snapshot(rtype::Snapshot{}).front());
        }
        server_udp.send_to(to, writer.data());
    };
    bool sent = false;
    server_udp.start_receiving([&](const Endpoint& from, std::span<const std::byte> /*bytes*/) {
        if (sent) {
            return;
        }
        sent = true;
        send(from, PacketType::Accept, 0);
        for (std::uint16_t sequence = 1; sequence <= 20; ++sequence) {
            if (sequence != 5 && sequence != 6 && sequence != 13) {
                send(from, PacketType::Snapshot, sequence);
            }
        }
    });
    listener.start_accepting([&](TcpConnection connection) {
        connections.push_back(std::make_unique<TcpConnection>(std::move(connection)));
        TcpConnection& accepted = *connections.back();
        accepted.start([&](std::span<const std::byte> message) {
            if (rtype::read_login_request(message)) {
                accepted.send(rtype::write_login_accepted(
                    rtype::LoginAccepted{.session = token,
                                         .player = 0,
                                         .udp_port = server_udp.local_endpoint().port()}));
            }
        });
    });

    ClientSession client{context, ClientSettings{.host = "127.0.0.1",
                                                 .tcp_port = listener.local_endpoint().port(),
                                                 .name = "R9"}};
    client.start();
    REQUIRE(pump(context, client,
                 [&] { return client.network_stats().summary(Clock::now()).received == 18; }));
    CHECK(client.state() == ClientState::InGame);
    const NetworkSummary summary = client.network_stats().summary(Clock::now());
    CHECK(summary.lost == 3);
    CHECK(summary.snapshots == 17);
}
