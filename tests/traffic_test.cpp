#include <aegis2d/core/log.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/traffic.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <rtype/client_session.hpp>
#include <rtype/events.hpp>
#include <rtype/input.hpp>
#include <rtype/login.hpp>
#include <rtype/server_network.hpp>
#include <rtype/snapshot.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <vector>

using aegis2d::net::Direction;
using aegis2d::net::NetworkContext;
using aegis2d::net::PacketHeader;
using aegis2d::net::PacketType;
using aegis2d::net::TrafficCount;
using aegis2d::net::TrafficCounters;
using Catch::Matchers::ContainsSubstring;
using rtype::ClientSession;
using rtype::ClientSettings;
using rtype::ClientState;
using rtype::ServerNetwork;

namespace {

// Runs the context, the server and the client until the condition holds, for at most ten
// seconds (a loaded CI runner can be seconds late).
template <typename Condition>
bool pump(NetworkContext& context, ServerNetwork& server, ClientSession& client,
          Condition condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        context.run_for(std::chrono::milliseconds{10});
        const auto now = ClientSession::Clock::now();
        server.update(now);
        client.update(now);
    }
    return true;
}

// Bytes of datagrams carrying these payloads, each after its own header.
std::uint64_t datagram_bytes(const std::vector<std::vector<std::byte>>& payloads) {
    std::uint64_t bytes = 0;
    for (const std::vector<std::byte>& payload : payloads) {
        bytes += PacketHeader::size + payload.size();
    }
    return bytes;
}

} // namespace

TEST_CASE("both ends count the datagrams of a known exchange", "[client][server][traffic]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    ClientSession client{context, ClientSettings{.host = "127.0.0.1",
                                                 .tcp_port = server.tcp_endpoint().port(),
                                                 .name = "R9"}};
    client.start();
    REQUIRE(pump(context, server, client, [&] { return client.state() == ClientState::InGame; }));
    const std::uint32_t token = client.login().value_or(rtype::LoginAccepted{}).session;
    const TrafficCounters* const on_server = server.traffic(token);
    REQUIRE(on_server != nullptr);
    const TrafficCounters& on_client = client.traffic();

    // Binding: as many Connect as the server read, each answered by an Accept; headers only.
    REQUIRE(pump(context, server, client, [&] {
        return on_server->total(Direction::Received, PacketType::Connect) ==
                   on_client.total(Direction::Sent, PacketType::Connect) &&
               on_client.total(Direction::Received, PacketType::Accept) ==
                   on_server->total(Direction::Sent, PacketType::Accept);
    }));
    const TrafficCount connects = on_client.total(Direction::Sent, PacketType::Connect);
    CHECK(connects.datagrams >= 1);
    CHECK(connects.bytes == PacketHeader::size * connects.datagrams);
    CHECK(on_server->total(Direction::Sent, PacketType::Accept) == connects);

    // Two snapshots of a known size.
    const std::vector<std::vector<std::byte>> snapshot = rtype::write_snapshot(rtype::Snapshot{});
    server.send_snapshot(snapshot, 0);
    server.send_snapshot(snapshot, 0);
    const TrafficCount snapshots{.datagrams = 2 * snapshot.size(),
                                 .bytes = 2 * datagram_bytes(snapshot)};
    CHECK(on_server->total(Direction::Sent, PacketType::Snapshot) == snapshots);
    REQUIRE(pump(context, server, client, [&] {
        return on_client.total(Direction::Received, PacketType::Snapshot) == snapshots;
    }));

    // An event: every Event datagram the server sent, resends included, reaches the client.
    const std::vector<rtype::GameEvent> events{
        rtype::PlayerLeftEvent{.tick = 1, .player = 3, .reason = rtype::LeaveReason::Left}};
    server.send_events(rtype::write_events(events), 0);
    REQUIRE(pump(context, server, client, [&] {
        const TrafficCount sent = on_server->total(Direction::Sent, PacketType::Event);
        return sent.datagrams >= 1 &&
               on_client.total(Direction::Received, PacketType::Event) == sent;
    }));

    // Inputs, one every tick: 18 bytes of header, then the newest input's sequence, the count
    // and the actions of 4 inputs, of 1 to 3 for the first three.
    REQUIRE(pump(context, server, client, [&] {
        return on_server->total(Direction::Received, PacketType::Input).datagrams >= 30;
    }));
    const TrafficCount inputs = on_server->total(Direction::Received, PacketType::Input);
    CHECK(inputs.bytes ==
          ((PacketHeader::size + rtype::input_payload_size(rtype::max_inputs_per_datagram)) *
           inputs.datagrams) -
              (3 + 2 + 1));
    CHECK(on_client.total(Direction::Sent, PacketType::Input).datagrams >= inputs.datagrams);
    // The client received the Accept, the snapshots and the events, nothing else.
    TrafficCount expected = connects;
    expected += snapshots;
    expected += on_client.total(Direction::Received, PacketType::Event);
    CHECK(on_client.total(Direction::Received) == expected);

    // The server logs each bound player's traffic every ten seconds.
    const auto log = std::filesystem::temp_directory_path() / "rtype_traffic_test.log";
    std::filesystem::remove(log);
    REQUIRE(aegis2d::core::Logger::global().open_file(log));
    server.update(ServerNetwork::Clock::now() + ServerNetwork::traffic_report_period);
    aegis2d::core::Logger::global().close_file();
    std::ifstream file{log};
    const std::string written{std::istreambuf_iterator<char>{file},
                              std::istreambuf_iterator<char>{}};
    CHECK_THAT(written, ContainsSubstring("UDP: player 0 sends"));
    CHECK_THAT(written, ContainsSubstring("datagrams/s"));
}
