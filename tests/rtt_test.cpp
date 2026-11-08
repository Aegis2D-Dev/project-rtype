#include <aegis2d/core/log.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/rtt.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <rtype/client_session.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/game_server.hpp>
#include <rtype/login.hpp>
#include <rtype/server_network.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using aegis2d::net::Endpoint;
using aegis2d::net::NetworkContext;
using aegis2d::net::RttEstimator;
using aegis2d::net::TcpConnection;
using aegis2d::net::TcpListener;
using aegis2d::net::UdpSocket;
using Catch::Matchers::ContainsSubstring;
using rtype::ClientSession;
using rtype::ClientSettings;
using rtype::ClientState;
using rtype::ServerNetwork;
using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

namespace {

// Stands between a client and a server as tools/netem.sh would: the login goes through
// unchanged but for the server's UDP port, which becomes the relay's, and every datagram,
// both ways, leaves the relay a fixed delay after it arrived.
class DelayedRelay {
public:
    DelayedRelay(NetworkContext& context, const ServerNetwork& server, Clock::duration delay)
        : server_tcp_{Endpoint::loopback(server.tcp_endpoint().port())},
          server_udp_{Endpoint::loopback(server.udp_endpoint().port())}, delay_{delay},
          listener_{context, 0}, client_side_{context, 0}, server_side_{context, 0} {
        listener_.start_accepting(
            [this, &context](TcpConnection connection) { accept(context, std::move(connection)); });
        client_side_.start_receiving(
            [this](const Endpoint& from, std::span<const std::byte> bytes) {
                client_ = from;
                hold(true, bytes);
            });
        server_side_.start_receiving(
            [this](const Endpoint& /*from*/, std::span<const std::byte> bytes) {
                hold(false, bytes);
            });
    }

    [[nodiscard]] std::uint16_t tcp_port() const { return listener_.local_endpoint().port(); }

    // Sends the datagrams whose delay is over, oldest first.
    void deliver(Clock::time_point now) {
        while (!held_.empty() && held_.front().due <= now) {
            const Held& datagram = held_.front();
            if (datagram.to_server) {
                server_side_.send_to(server_udp_, datagram.bytes);
            } else {
                client_side_.send_to(client_, datagram.bytes);
            }
            held_.pop_front();
        }
    }

private:
    struct Held {
        Clock::time_point due;
        bool to_server = false;
        std::vector<std::byte> bytes;
    };
    // A client's login connection and the relay's own one to the server.
    struct Link {
        Link(TcpConnection accepted, NetworkContext& context)
            : client{std::move(accepted)}, server{context} {}
        TcpConnection client;
        TcpConnection server;
        bool connected = false;
        std::vector<std::vector<std::byte>> waiting;
    };

    // Every datagram waits the same delay: the queue stays in the order of their due times.
    void hold(bool to_server, std::span<const std::byte> bytes) {
        held_.push_back(Held{.due = Clock::now() + delay_,
                             .to_server = to_server,
                             .bytes = {bytes.begin(), bytes.end()}});
    }

    void accept(NetworkContext& context, TcpConnection connection) {
        links_.push_back(std::make_unique<Link>(std::move(connection), context));
        Link& link = *links_.back();
        link.client.start([&link](std::span<const std::byte> message) {
            if (link.connected) {
                link.server.send(message);
            } else {
                link.waiting.emplace_back(message.begin(), message.end());
            }
        });
        link.server.connect(server_tcp_, [this, &link](const std::error_code& error) {
            if (error) {
                return;
            }
            link.connected = true;
            link.server.start([this, &link](std::span<const std::byte> message) {
                const std::optional<rtype::LoginAccepted> accepted =
                    rtype::read_login_accepted(message);
                if (!accepted.has_value()) {
                    link.client.send(message);
                    return;
                }
                rtype::LoginAccepted relayed = accepted.value_or(rtype::LoginAccepted{});
                relayed.udp_port = client_side_.local_endpoint().port();
                link.client.send(rtype::write_login_accepted(relayed));
            });
            for (const std::vector<std::byte>& message : link.waiting) {
                link.server.send(message);
            }
            link.waiting.clear();
        });
    }

    Endpoint server_tcp_;
    Endpoint server_udp_;
    Clock::duration delay_;
    TcpListener listener_;
    UdpSocket client_side_;
    UdpSocket server_side_;
    Endpoint client_;
    std::deque<Held> held_;
    std::vector<std::unique_ptr<Link>> links_;
};

// Runs the context, the relay if any, the server and the client until the condition holds,
// for at most ten seconds (a loaded CI runner can be seconds late). It polls without
// sleeping: a sleep of a millisecond lasts about 15 on Windows, which the relay and both
// ends would add to every round trip.
template <typename Condition>
bool pump(NetworkContext& context, DelayedRelay* relay, ServerNetwork& server,
          ClientSession& client, Condition condition) {
    const auto deadline = Clock::now() + std::chrono::seconds{10};
    while (!condition()) {
        if (Clock::now() > deadline) {
            return false;
        }
        context.poll();
        const auto now = Clock::now();
        if (relay != nullptr) {
            relay->deliver(now);
        }
        server.update(now);
        client.update(now);
    }
    return true;
}

// Runs them for a duration of real time.
void run_for(NetworkContext& context, DelayedRelay* relay, ServerNetwork& server,
             ClientSession& client, Clock::duration duration) {
    const auto end = Clock::now() + duration;
    pump(context, relay, server, client, [&] { return Clock::now() >= end; });
}

milliseconds whole_ms(Clock::duration duration) {
    return std::chrono::duration_cast<milliseconds>(duration);
}

} // namespace

TEST_CASE("both ends estimate 200 ms and the peer's wait with 100 ms of delay each way",
          "[client][server][rtt]") {
    // A game, so that the server sends snapshots 20 times per second, as in a real match.
    NetworkContext context;
    rtype::GameServer game_server{context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0};
    ServerNetwork& server = game_server.network();
    DelayedRelay relay{context, server, milliseconds{100}};
    ClientSession client{
        context, ClientSettings{.host = "127.0.0.1", .tcp_port = relay.tcp_port(), .name = "R9"}};
    client.start();
    REQUIRE(pump(context, &relay, server, client,
                 [&] { return client.state() == ClientState::InGame; }));
    const std::uint32_t token = client.login().value_or(rtype::LoginAccepted{}).session;
    const RttEstimator* const on_server = server.rtt(token);
    REQUIRE(on_server != nullptr);
    const RttEstimator& on_client = client.rtt();

    // Three seconds: about 60 samples on the client, one per snapshot, and as many on the
    // server, one per Input acknowledging a new snapshot. The first sample sets the
    // estimate and each next one moves it by an eighth: it has settled long before the end.
    run_for(context, &relay, server, client, std::chrono::seconds{3});
    CAPTURE(whole_ms(on_client.smoothed()), whole_ms(on_client.jitter()), on_client.samples(),
            whole_ms(on_server->smoothed()), whole_ms(on_server->jitter()), on_server->samples());
    REQUIRE(on_client.samples() >= 30);
    REQUIRE(on_server->samples() >= 30);

    // A sample is the delay plus the time the peer waited before acknowledging: the server
    // until the snapshot after the last Input, the client until its next Input. Both tick at
    // 60 Hz, so each wait stays under an input interval (on an idle machine, the two add up
    // to one). The margin is for a loaded runner, or one slowed by the sanitizers.
    const milliseconds delay{200};
    const auto interval =
        std::chrono::duration_cast<Clock::duration>(ClientSession::input_interval);
    const milliseconds margin{15};
    CHECK(on_client.smoothed() >= delay);
    CHECK(on_client.smoothed() <= delay + interval + margin);
    CHECK(on_server->smoothed() >= delay);
    CHECK(on_server->smoothed() <= delay + interval + margin);
    CHECK(on_client.jitter() < milliseconds{20});
    CHECK(on_server->jitter() < milliseconds{20});
}

TEST_CASE("the round trip is measured from the binding and logged by the server",
          "[client][server][rtt]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    ClientSession client{context, ClientSettings{.host = "127.0.0.1",
                                                 .tcp_port = server.tcp_endpoint().port(),
                                                 .name = "R9"}};
    CHECK_FALSE(client.rtt().has_estimate());
    client.start();
    REQUIRE(pump(context, nullptr, server, client,
                 [&] { return client.state() == ClientState::InGame; }));
    // The Accept acknowledges the Connect it answers: the client has its first sample.
    CHECK(client.rtt().has_estimate());
    CHECK(client.rtt().smoothed() < milliseconds{100});
    const std::uint32_t token = client.login().value_or(rtype::LoginAccepted{}).session;
    CHECK(server.rtt(token + 1) == nullptr);
    const RttEstimator* const on_server = server.rtt(token);
    REQUIRE(on_server != nullptr);
    // The first Input acknowledges the Accept: the server has its first sample.
    REQUIRE(pump(context, nullptr, server, client, [&] { return on_server->has_estimate(); }));
    CHECK(on_server->smoothed() < milliseconds{100});

    // The traffic line of each bound player ends with its round trip time.
    const auto log = std::filesystem::temp_directory_path() / "rtype_rtt_test.log";
    std::filesystem::remove(log);
    REQUIRE(aegis2d::core::Logger::global().open_file(log));
    server.update(ServerNetwork::Clock::now() + ServerNetwork::traffic_report_period);
    aegis2d::core::Logger::global().close_file();
    std::ifstream file{log};
    const std::string written{std::istreambuf_iterator<char>{file},
                              std::istreambuf_iterator<char>{}};
    CHECK_THAT(written, ContainsSubstring("UDP: player 0 sends"));
    CHECK_THAT(written, ContainsSubstring("B received in all; round trip "));
    CHECK_THAT(written, ContainsSubstring(" ms (jitter "));
}
