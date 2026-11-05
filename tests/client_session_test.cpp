#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/system.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/binary.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/rtt.hpp>
#include <aegis2d/net/sequence.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/traffic.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/delta_snapshot.hpp>
#include <rtype/entities.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/game_server.hpp>
#include <rtype/input.hpp>
#include <rtype/lag_compensation.hpp>
#include <rtype/login.hpp>
#include <rtype/room_list_form.hpp>
#include <rtype/rooms.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>
#include <rtype/ship_movement.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/snapshot_history.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using aegis2d::ecs::Action;
using aegis2d::ecs::PlayerInput;
using aegis2d::net::Endpoint;
using aegis2d::net::NetworkContext;
using aegis2d::net::PacketHeader;
using aegis2d::net::PacketType;
using aegis2d::net::TcpConnection;
using aegis2d::net::TcpListener;
using aegis2d::net::UdpSocket;
using Catch::Matchers::ContainsSubstring;
using rtype::ClientSession;
using rtype::ClientSettings;
using rtype::ClientState;
using rtype::ServerNetwork;

namespace {

// Runs the context, the server and the clients until the condition holds, for at most wait
// (ten seconds by default: Windows reports a refused connection after about two, and a loaded
// CI runner can be seconds late), the clients' clock being shifted by offset.
template <typename Condition>
bool pump(NetworkContext& context, ServerNetwork* server,
          std::initializer_list<ClientSession*> clients, Condition condition,
          ClientSession::Clock::duration offset = {},
          std::chrono::milliseconds wait = std::chrono::seconds{10}) {
    const auto deadline = std::chrono::steady_clock::now() + wait;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        context.run_for(std::chrono::milliseconds{10});
        const auto now = ClientSession::Clock::now();
        if (server != nullptr) {
            server->update(now);
        }
        for (ClientSession* const client : clients) {
            client->update(now + offset);
        }
    }
    return true;
}

ClientSettings settings_for(const ServerNetwork& server, std::string name) {
    return ClientSettings{
        .host = "127.0.0.1", .tcp_port = server.tcp_endpoint().port(), .name = std::move(name)};
}

} // namespace

TEST_CASE("the client logs in, binds its endpoint and reaches the game", "[client]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<unsigned> joined;
    server.on_players([&](const rtype::Session& session) { joined.push_back(session.player); },
                      [](const rtype::Session& /*session*/, rtype::LeaveReason /*reason*/) {});
    ClientSession client{context, settings_for(server, "R9")};
    CHECK(client.state() == ClientState::Idle);

    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    REQUIRE(client.login().has_value());
    const rtype::LoginAccepted login = client.login().value_or(rtype::LoginAccepted{});
    CHECK(login.player == 0);
    CHECK(login.udp_port == server.udp_endpoint().port());
    CHECK(client.server_udp() == std::optional{Endpoint::loopback(server.udp_endpoint().port())});
    CHECK(client.connects_sent() >= 1);
    CHECK(client.error().empty());
    const rtype::Session* const session = server.sessions().find(login.session);
    REQUIRE(session != nullptr);
    CHECK(session->name == "R9");
    CHECK(session->endpoint.has_value());
    CHECK(joined == std::vector<unsigned>{0});

    // Leaving: Disconnect and Logout end the session on the server.
    client.leave();
    CHECK(client.state() == ClientState::Closed);
    REQUIRE(pump(context, &server, {&client}, [&] { return server.sessions().size() == 0; }));
}

TEST_CASE("a refused login tells the player why", "[client]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    ClientSession first{context, settings_for(server, "R9")};
    first.start();
    REQUIRE(pump(context, &server, {&first}, [&] { return first.state() == ClientState::InGame; }));

    ClientSession twin{context, settings_for(server, "R9")};
    twin.start();
    REQUIRE(pump(context, &server, {&first, &twin},
                 [&] { return twin.state() == ClientState::Failed; }));
    CHECK(twin.error() == "another player has this name");
    CHECK(!twin.login().has_value());
    CHECK(first.state() == ClientState::InGame);
}

TEST_CASE("a fifth player is told the server is full", "[client]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<std::unique_ptr<ClientSession>> players;
    for (int index = 0; index < 4; ++index) {
        players.push_back(std::make_unique<ClientSession>(
            context, settings_for(server, "p" + std::to_string(index))));
        players.back()->start();
    }
    ClientSession fifth{context, settings_for(server, "p4")};
    REQUIRE(pump(context, &server, {}, [&] { return server.sessions().size() == 4; }));
    fifth.start();
    REQUIRE(pump(context, &server, {&fifth}, [&] { return fifth.state() == ClientState::Failed; }));
    CHECK(fifth.error() == "the server is full");
}

TEST_CASE("bad settings and an absent server fail with a reason", "[client]") {
    NetworkContext context;
    ClientSession hostname{context,
                           ClientSettings{.host = "example.com", .tcp_port = 4242, .name = "R9"}};
    hostname.start();
    CHECK(hostname.state() == ClientState::Failed);
    CHECK(hostname.error() == "\"example.com\" is not an IP address");

    ClientSession bad_name{
        context, ClientSettings{.host = "127.0.0.1", .tcp_port = 4242, .name = "tab\there"}};
    bad_name.start();
    CHECK(bad_name.state() == ClientState::Failed);
    CHECK_THAT(bad_name.error(), ContainsSubstring("the name is not valid"));

    // A port nobody listens on: a listener is opened then closed to find one.
    std::uint16_t closed_port = 0;
    {
        const TcpListener probe{context, 0};
        closed_port = probe.local_endpoint().port();
    }
    ClientSession absent{
        context, ClientSettings{.host = "localhost", .tcp_port = closed_port, .name = "R9"}};
    absent.start();
    CHECK(absent.state() == ClientState::Connecting);
    REQUIRE(
        pump(context, nullptr, {&absent}, [&] { return absent.state() == ClientState::Failed; }));
    CHECK_THAT(absent.error(), ContainsSubstring("cannot reach 127.0.0.1:"));
}

TEST_CASE("a blocked UDP port ends in a Logout and a firewall hint", "[client]") {
    // A server that logs in but never answers over UDP, as behind a firewall.
    NetworkContext context;
    TcpListener listener{context, 0};
    UdpSocket silent{context, 0};
    std::vector<std::unique_ptr<TcpConnection>> connections;
    std::vector<std::vector<std::byte>> received;
    std::uint64_t datagrams = 0;
    silent.start_receiving(
        [&](const Endpoint& /*from*/, std::span<const std::byte> /*bytes*/) { ++datagrams; });
    listener.start_accepting([&](TcpConnection connection) {
        connections.push_back(std::make_unique<TcpConnection>(std::move(connection)));
        TcpConnection& accepted = *connections.back();
        accepted.start([&](std::span<const std::byte> message) {
            received.emplace_back(message.begin(), message.end());
            if (rtype::read_login_request(message)) {
                accepted.send(rtype::write_login_accepted(rtype::LoginAccepted{
                    .session = 7, .player = 1, .udp_port = silent.local_endpoint().port()}));
            }
        });
    });

    ClientSession client{context, ClientSettings{.host = "127.0.0.1",
                                                 .tcp_port = listener.local_endpoint().port(),
                                                 .name = "R9"}};
    client.start();
    REQUIRE(
        pump(context, nullptr, {&client}, [&] { return client.state() == ClientState::Binding; }));
    REQUIRE(pump(context, nullptr, {&client}, [&] { return datagrams >= 2; }));
    CHECK(client.connects_sent() >= 2);

    // Five seconds later: Logout, and the player is told why.
    REQUIRE(pump(
        context, nullptr, {&client}, [&] { return client.state() == ClientState::Failed; },
        ClientSession::bind_timeout));
    CHECK(client.error() == "cannot reach the server over UDP (port " +
                                std::to_string(silent.local_endpoint().port()) +
                                "): a firewall may block it");
    REQUIRE(pump(context, nullptr, {&client}, [&] { return received.size() == 2; }));
    CHECK(received.back() == rtype::write_logout(rtype::LogoutReason::Left));
}

TEST_CASE("a server that stops loses the session with its reason", "[client][lost]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    server.shutdown();
    REQUIRE(pump(context, &server, {&client}, [&] { return client.state() == ClientState::Lost; }));
    CHECK(client.error() == "the server stopped");
}

TEST_CASE("a server that crashes loses the session", "[client][lost]") {
    NetworkContext context;
    auto server = std::make_unique<ServerNetwork>(context, 0, 0);
    ClientSession client{context, settings_for(*server, "R9")};
    client.start();
    REQUIRE(pump(context, server.get(), {&client},
                 [&] { return client.state() == ClientState::InGame; }));
    // Gone without a Logout: its connections close with it.
    server.reset();
    REQUIRE(pump(context, nullptr, {&client}, [&] { return client.state() == ClientState::Lost; }));
    CHECK(client.error() == "the server closed the connection");
}

TEST_CASE("a server silent for five seconds loses the session", "[client][lost]") {
    // A ServerNetwork alone answers Connect but sends nothing else: no game, no snapshot.
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));

    // A little less than the timeout: still in game.
    REQUIRE(!pump(
        context, &server, {&client}, [&] { return client.state() != ClientState::InGame; },
        ClientSession::server_timeout - std::chrono::milliseconds{500},
        std::chrono::milliseconds{200}));
    REQUIRE(pump(
        context, &server, {&client}, [&] { return client.state() == ClientState::Lost; },
        ClientSession::server_timeout));
    CHECK(client.error() == "connection lost: the server sent nothing for 5 seconds");
    // The server is told over TCP, in case only the UDP traffic was lost.
    REQUIRE(pump(context, &server, {&client}, [&] { return server.sessions().size() == 0; }));
}

TEST_CASE("a game that keeps sending keeps the session", "[client][lost]") {
    NetworkContext context;
    rtype::GameServer game_server{context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0};
    ServerNetwork& server = game_server.network();
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    // Snapshots arrive 20 times per second: even 4.5 seconds ahead, the server is not silent.
    CHECK(!pump(
        context, &server, {&client}, [&] { return client.state() != ClientState::InGame; },
        ClientSession::server_timeout - std::chrono::milliseconds{500},
        std::chrono::milliseconds{300}));
    CHECK(client.state() == ClientState::InGame);
}

namespace {

std::variant<rtype::ClientOptions, std::string>
parse_client(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> list{args};
    return rtype::parse_client_options(list);
}

} // namespace

TEST_CASE("the client reads its server and name from the command line", "[client][options]") {
    const auto defaults = parse_client({});
    REQUIRE(std::holds_alternative<rtype::ClientOptions>(defaults));
    const ClientSettings standard = std::get<rtype::ClientOptions>(defaults).settings;
    CHECK(standard.host == "127.0.0.1");
    CHECK(standard.tcp_port == 4242);
    CHECK(standard.name == "player");

    const auto given = parse_client({"--name", "R9", "--port", "5000", "--host", "::1"});
    REQUIRE(std::holds_alternative<rtype::ClientOptions>(given));
    const ClientSettings settings = std::get<rtype::ClientOptions>(given).settings;
    CHECK(settings.host == "::1");
    CHECK(settings.tcp_port == 5000);
    CHECK(settings.name == "R9");

    CHECK(std::get<rtype::ClientOptions>(parse_client({"--version", "--bad"})).command ==
          rtype::ClientCommand::Version);
    CHECK(std::get<rtype::ClientOptions>(parse_client({"--help"})).command ==
          rtype::ClientCommand::Help);
    CHECK(std::get<std::string>(parse_client({"--port", "0"})) ==
          "--port: \"0\" is not a port (1 to 65535)");
    CHECK(std::get<std::string>(parse_client({"--name"})) == "--name needs a value");
    CHECK(std::get<std::string>(parse_client({"--server"})) == "unknown argument \"--server\"");
}

TEST_CASE("the client reads its window options from the command line", "[client][options]") {
    const auto defaults = std::get<rtype::ClientOptions>(parse_client({}));
    CHECK_FALSE(defaults.join);
    CHECK_FALSE(defaults.settings_file.has_value());
    CHECK(defaults.frames == 0);
    CHECK_FALSE(defaults.screenshot.has_value());

    const auto given = std::get<rtype::ClientOptions>(parse_client(
        {"--join", "--settings", "me.json", "--frames", "90", "--screenshot", "shot.png"}));
    CHECK(given.join);
    CHECK(given.settings_file == std::filesystem::path{"me.json"});
    CHECK(given.frames == 90);
    CHECK(given.screenshot == std::filesystem::path{"shot.png"});

    // A screenshot without --frames is taken after 300 frames.
    CHECK(std::get<rtype::ClientOptions>(parse_client({"--screenshot", "a.png"})).frames == 300);
    CHECK(std::get<std::string>(parse_client({"--frames", "0"})) ==
          "--frames: \"0\" is not a number of frames");
    CHECK(std::get<std::string>(parse_client({"--frames", "ten"})) ==
          "--frames: \"ten\" is not a number of frames");

    CHECK(rtype::parse_port("4242") == std::uint16_t{4242});
    CHECK_FALSE(rtype::parse_port("+42").has_value());
    CHECK_FALSE(rtype::parse_port("65536").has_value());
}

namespace {

// Runs the context, the server and the client for a duration of real time.
void run_for(NetworkContext& context, ServerNetwork& server, ClientSession& client,
             std::chrono::milliseconds duration) {
    const auto end = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < end) {
        context.run_for(std::chrono::milliseconds{1});
        const auto now = ClientSession::Clock::now();
        server.update(now);
        client.update(now);
    }
}

PlayerInput holding(Action action) {
    PlayerInput input;
    input.set(action, true);
    return input;
}

} // namespace

TEST_CASE("the server receives the held actions 60 times per second", "[client][input]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<std::uint8_t> inputs;
    server.on_input([&](const rtype::Session& /*session*/, PlayerInput input,
                        std::uint16_t /*sequence*/) { inputs.push_back(input.held); });
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    CHECK(client.inputs_sent() <= 1);

    // Bits 5 to 7 are not actions: the client clears them, so the server takes the input.
    PlayerInput right = holding(Action::Right);
    right.held |= 0xE0U;
    client.set_input(right);
    // The schedule starts when the game state is reached: let it settle before measuring.
    run_for(context, server, client, std::chrono::milliseconds{100});
    const std::size_t before = inputs.size();
    const std::uint64_t sent_before = client.inputs_sent();
    const auto start = std::chrono::steady_clock::now();
    run_for(context, server, client, std::chrono::milliseconds{1000});
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    const std::uint64_t sent = client.inputs_sent() - sent_before;
    // A little more time for the last ones to arrive.
    run_for(context, server, client, std::chrono::milliseconds{50});

    const double rate = static_cast<double>(sent) / elapsed.count();
    CAPTURE(rate, inputs.size() - before);
    CHECK(rate > 50.0);
    CHECK(rate < 70.0);
    // Each Input of the second was more recent than the last, so the server passed each one
    // on; the client keeps sending during the extra time, and its very last ones may still
    // be on their way.
    CHECK(inputs.size() - before >= sent);
    REQUIRE(inputs.size() > before);
    CHECK(inputs.back() == 0x08);
    CHECK(server.dropped_datagrams() == 0);
}

TEST_CASE("inputs acknowledge the datagrams of the server", "[client][input]") {
    // A server that numbers its datagrams: Accept is 0, then three Snapshot datagrams: tick 3
    // whole, tick 6 against it, and tick 9 against a tick 4 the client never received.
    NetworkContext context;
    TcpListener listener{context, 0};
    UdpSocket server_udp{context, 0};
    std::vector<std::unique_ptr<TcpConnection>> connections;
    std::vector<PacketHeader> received;
    std::vector<std::uint8_t> actions;
    std::vector<std::vector<rtype::InputRecord>> inputs;
    constexpr std::uint32_t token = 77;
    const auto send_header = [&](const Endpoint& to, PacketType type, std::uint16_t sequence,
                                 std::uint16_t ack) {
        aegis2d::net::BinaryWriter writer;
        aegis2d::net::write_header(
            writer,
            PacketHeader{
                .type = type, .sequence = sequence, .ack = ack, .ack_bits = 0, .session = token});
        if (type == PacketType::Snapshot) {
            // Real snapshots: the client acknowledges only the datagrams it could read and
            // rebuild.
            const auto at = [](std::uint32_t tick) {
                return rtype::Snapshot{.tick = tick, .players = {}, .entities = {}};
            };
            const rtype::Snapshot baseline = at(sequence == 2 ? 3 : 4);
            writer.write_bytes(
                rtype::write_delta_snapshot(at(sequence * 3), sequence == 1 ? nullptr : &baseline)
                    .front());
        }
        server_udp.send_to(to, writer.data());
    };
    server_udp.start_receiving([&](const Endpoint& from, std::span<const std::byte> bytes) {
        aegis2d::net::BinaryReader reader{bytes};
        const auto read = aegis2d::net::read_header(reader);
        const auto* const header = std::get_if<PacketHeader>(&read);
        if (header == nullptr) {
            return;
        }
        received.push_back(*header);
        if (header->type == PacketType::Connect) {
            send_header(from, PacketType::Accept, 0, header->sequence);
            send_header(from, PacketType::Snapshot, 1, header->sequence);
            send_header(from, PacketType::Snapshot, 2, header->sequence);
            send_header(from, PacketType::Snapshot, 3, header->sequence);
        } else if (header->type == PacketType::Input) {
            const std::optional<std::vector<rtype::InputRecord>> read =
                rtype::read_inputs(std::span{bytes}.subspan(PacketHeader::size));
            REQUIRE(read.has_value());
            inputs.push_back(read.value_or(std::vector<rtype::InputRecord>{}));
            actions.push_back(inputs.back().back().actions);
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
    client.set_input(holding(Action::Fire));
    REQUIRE(pump(context, nullptr, {&client}, [&] { return actions.size() >= 5; }));

    // The client's sequences follow each other from its first Connect, and its last Input
    // acknowledges the Snapshot of sequence 2 and, in its bits, 1 and the Accept; not 3,
    // whose baseline it does not have.
    CHECK(client.snapshots().find(6) != nullptr);
    CHECK(client.snapshots().find(9) == nullptr);
    CHECK(client.snapshots().dropped() == 1);
    const PacketHeader& last = received.back();
    CHECK(last.type == PacketType::Input);
    CHECK(last.session == token);
    CHECK(last.ack == 2);
    CHECK(last.ack_bits == 0b11U);
    for (std::size_t index = 1; index < received.size(); ++index) {
        CHECK(received.at(index).sequence == received.at(index - 1).sequence + 1);
    }
    CHECK(actions.back() == 0x10);

    // Each Input carries the 3 inputs before its own, but the first three.
    REQUIRE(inputs.size() >= 4);
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        CAPTURE(index);
        const std::vector<rtype::InputRecord>& carried = inputs.at(index);
        REQUIRE(carried.size() == std::min<std::size_t>(index + 1, 4));
        for (std::size_t back = 0; back < carried.size(); ++back) {
            CHECK(carried.at(carried.size() - 1 - back).sequence == index - back);
        }
    }
}

TEST_CASE("the input the snapshots acknowledge follows the inputs sent", "[client][input]") {
    NetworkContext context;
    rtype::GameServer game_server{context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0};
    ServerNetwork& server = game_server.network();
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    CHECK(!client.acknowledged_input().has_value());
    client.set_input(holding(Action::Right));

    // At each step, a new input is sent, then the acknowledged input catches up with it; it
    // never goes past the inputs sent, nor back.
    std::uint16_t previous = 0;
    std::optional<std::uint16_t> sent;
    bool checked = true;
    for (int step = 0; step < 5; ++step) {
        REQUIRE(pump(context, &server, {&client}, [&] {
            return client.last_input().has_value() && client.last_input() != sent;
        }));
        sent = client.last_input();
        const std::uint16_t target = sent.value_or(0);
        REQUIRE(pump(context, &server, {&client}, [&] {
            const std::optional<std::uint16_t> acknowledged = client.acknowledged_input();
            if (!acknowledged) {
                return false;
            }
            const std::uint16_t value = *acknowledged;
            checked =
                checked &&
                !aegis2d::net::sequence_greater_than(value, client.last_input().value_or(0)) &&
                !aegis2d::net::sequence_less_than(value, previous);
            previous = value;
            return !aegis2d::net::sequence_less_than(value, target);
        }));
    }
    CHECK(checked);
    CHECK(client.inputs_sent() >= 5);
    // The server's game applied the same input: the one of the player's record.
    game_server.simulation().stop();
    rtype::GameInstance& instance = game_server.room().instance();
    const std::optional<rtype::PlayerState> state = instance.player_state(0);
    REQUIRE(state.has_value());
    CHECK(state.value_or(rtype::PlayerState{}).input.has_value());
    CHECK(!aegis2d::net::sequence_less_than(state.value_or(rtype::PlayerState{}).input.value_or(0),
                                            previous));
}

TEST_CASE("at 150 ms of round trip the own ship answers the keys within a frame",
          "[client][prediction]") {
    // A server 150 ms away: it binds the endpoint and sends the world once, the player's ship
    // at (100, 360); nothing it sends after the key press can arrive within the 150 ms.
    NetworkContext context;
    TcpListener listener{context, 0};
    UdpSocket server_udp{context, 0};
    std::vector<std::unique_ptr<TcpConnection>> connections;
    constexpr std::uint32_t token = 77;
    constexpr std::uint32_t ship = 7;
    server_udp.start_receiving([&](const Endpoint& from, std::span<const std::byte> bytes) {
        aegis2d::net::BinaryReader reader{bytes};
        const auto read = aegis2d::net::read_header(reader);
        const auto* const header = std::get_if<PacketHeader>(&read);
        if (header == nullptr) {
            return;
        }
        if (header->type != PacketType::Connect) {
            return;
        }
        for (const PacketType type : {PacketType::Accept, PacketType::Snapshot}) {
            aegis2d::net::BinaryWriter writer;
            aegis2d::net::write_header(writer, PacketHeader{.type = type,
                                                            .sequence = type == PacketType::Accept
                                                                            ? std::uint16_t{0}
                                                                            : std::uint16_t{1},
                                                            .ack = header->sequence,
                                                            .ack_bits = 0,
                                                            .session = token});
            if (type == PacketType::Snapshot) {
                const rtype::Snapshot world{
                    .tick = 3,
                    .players = {rtype::PlayerRecord{
                        .player = 0, .ship = ship, .lives = 3, .score = 0, .input = std::nullopt}},
                    .entities = {rtype::EntityRecord{.id = ship,
                                                     .kind = 1,
                                                     .x = 100.0F,
                                                     .y = 360.0F,
                                                     .vx = 0.0F,
                                                     .vy = 0.0F,
                                                     .frame = 0}}};
                writer.write_bytes(rtype::write_delta_snapshot(world, nullptr).front());
            }
            server_udp.send_to(from, writer.data());
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
    REQUIRE(pump(context, nullptr, {&client},
                 [&] { return client.prediction().position().has_value(); }));
    const aegis2d::Vec2 start = client.prediction().position().value_or(aegis2d::Vec2{});
    CHECK(start == aegis2d::Vec2{100.0F, 360.0F});
    CHECK(client.prediction().ship() == std::optional{ship});

    // The key goes down; one frame later at 60 frames per second, the ship has moved, as the
    // server will move it, and goes on at each frame of the 150 ms.
    const aegis2d::ecs::PlayerInput right = holding(Action::Right);
    client.set_input(right);
    const auto pressed = ClientSession::Clock::now();
    const float tick =
        aegis2d::ecs::Tick{.step = aegis2d::core::step_from_rate(rtype::tuning::tick_rate),
                           .index = 0}
            .delta();
    aegis2d::Vec2 expected = start;
    const std::uint64_t sent = client.inputs_sent();
    for (int frame = 1; frame <= 9; ++frame) {
        client.update(pressed + (frame * std::chrono::milliseconds{17}));
        CAPTURE(frame);
        REQUIRE(client.inputs_sent() == sent + static_cast<std::uint64_t>(frame));
        expected = rtype::step_ship(expected, right, tick).position;
        CHECK(client.prediction().position() == std::optional{expected});
    }
    CHECK(expected.x > start.x + 40.0F);
    // Nothing came back: the server has applied none of them, the client keeps them all.
    CHECK(client.prediction().pending().size() >= 9);
    CHECK(client.snapshots().size() == 1);
}

TEST_CASE("reconciled with the snapshots, the own ship ends where the server's is",
          "[client][reconciliation]") {
    NetworkContext context;
    rtype::GameServer game_server{context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0};
    ServerNetwork& server = game_server.network();
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(pump(context, &server, {&client},
                 [&] { return client.prediction().position().has_value(); }));

    // Up and right for a while, then nothing: once the server has applied every input, the
    // ship stands still on both sides.
    PlayerInput diagonal = holding(Action::Up);
    diagonal.set(Action::Right, true);
    client.set_input(diagonal);
    run_for(context, server, client, std::chrono::milliseconds{400});
    client.set_input(PlayerInput{});
    run_for(context, server, client, std::chrono::milliseconds{100});
    const std::uint16_t released = client.last_input().value_or(0);
    REQUIRE(pump(context, &server, {&client}, [&] {
        const std::optional<std::uint16_t> acknowledged = client.acknowledged_input();
        return acknowledged.has_value() &&
               !aegis2d::net::sequence_less_than(acknowledged.value_or(0), released);
    }));
    run_for(context, server, client, std::chrono::milliseconds{100});

    game_server.simulation().stop();
    rtype::GameInstance& instance = game_server.room().instance();
    const std::optional<aegis2d::ecs::Entity> ship = instance.ship(0);
    REQUIRE(ship.has_value());
    const aegis2d::Vec2 at =
        instance.game()
            .registry()
            .get<aegis2d::ecs::Transform>(ship.value_or(aegis2d::ecs::Entity{}))
            .position;
    const aegis2d::Vec2 predicted = client.prediction().position().value_or(aegis2d::Vec2{});
    CAPTURE(at.x, at.y, predicted.x, predicted.y, client.prediction().corrections());
    CHECK(at.x > rtype::player_spawn(0).x + 50.0F);
    // Within the rounding of the snapshots, 1/16 pixel.
    CHECK(std::abs(predicted.x - at.x) <= 0.0625F);
    CHECK(std::abs(predicted.y - at.y) <= 0.0625F);
    CHECK(client.prediction().pending().size() <= 3);
}

TEST_CASE("the server compensates each player by its round trip time", "[client][lag]") {
    NetworkContext context;
    rtype::GameServer game_server{context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0};
    ServerNetwork& server = game_server.network();
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    const std::uint32_t token = client.login().value_or(rtype::LoginAccepted{}).session;
    REQUIRE(pump(context, &server, {&client}, [&] {
        const aegis2d::net::RttEstimator* const rtt = server.rtt(token);
        return rtt != nullptr && rtt->has_estimate();
    }));
    run_for(context, server, client, std::chrono::milliseconds{300});

    // On loopback the round trip is a few milliseconds at most: the rewind is about the
    // client's interpolation delay, 6 ticks.
    game_server.simulation().stop();
    rtype::GameInstance& instance = game_server.room().instance();
    const aegis2d::net::RttEstimator* const rtt = server.rtt(token);
    REQUIRE(rtt != nullptr);
    const std::optional<aegis2d::ecs::Entity> ship = instance.ship(0);
    REQUIRE(ship.has_value());
    // Within a tick of the latest estimate: the last change may still wait in the queue of
    // the simulation when it stops.
    const auto rewind =
        static_cast<std::int64_t>(instance.game().rewind(ship.value_or(aegis2d::ecs::Entity{})));
    const auto expected = static_cast<std::int64_t>(
        rtype::rewind_ticks(rtt->smoothed(), rtype::tuning::client_interpolation_delay));
    CHECK(std::abs(rewind - expected) <= 1);
    CHECK(rewind >= 6);
}

TEST_CASE("the client reads the compressed snapshots of the server", "[client][compression]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    server.set_compression(128);
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    const std::uint32_t token = client.login().value_or(rtype::LoginAccepted{}).session;

    // 100 still entities, whole as there is no baseline yet: one payload of about 1 KB of
    // records alike, which LZ4 shrinks.
    rtype::Snapshot world{.tick = 3, .players = {}, .entities = {}};
    for (std::uint32_t id = 1; id <= 100; ++id) {
        world.entities.push_back(rtype::EntityRecord{
            .id = id, .kind = 3, .x = 640.0F, .y = 360.0F, .vx = 0.0F, .vy = 0.0F, .frame = 0});
    }
    const auto snapshot = std::make_shared<const rtype::Snapshot>(world);
    const std::vector<std::vector<std::byte>> payloads =
        rtype::write_delta_snapshot(*snapshot, nullptr);
    REQUIRE(payloads.size() == 1);
    server.send_snapshot(snapshot, 0);
    // The client acknowledges only the datagrams whose payload it could read (RFC 5.1).
    REQUIRE(pump(context, &server, {&client}, [&] {
        const rtype::SnapshotHistory* const history = server.snapshots(token);
        return history != nullptr && history->latest_acknowledged() != nullptr;
    }));
    CHECK(client.snapshots().find(3) != nullptr);
    const aegis2d::net::TrafficCount sent =
        server.traffic(token)->total(aegis2d::net::Direction::Sent, PacketType::Snapshot);
    CHECK(sent.datagrams == 1);
    CHECK(sent.bytes < PacketHeader::size + payloads.front().size());
    CHECK(client.traffic().total(aegis2d::net::Direction::Received, PacketType::Snapshot).bytes ==
          sent.bytes);
}

TEST_CASE("a client holding right moves its ship in the server's game", "[client][input]") {
    NetworkContext context;
    rtype::GameServer game_server{context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0};
    ServerNetwork& server = game_server.network();
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    client.set_input(holding(Action::Right));
    run_for(context, server, client, std::chrono::milliseconds{500});

    // Stopped, the game is the test's to read.
    game_server.simulation().stop();
    rtype::GameInstance& instance = game_server.room().instance();
    // The server sends delta snapshots, which the client rebuilds as the server took them.
    const rtype::SnapshotHistory* const history =
        server.snapshots(client.login().value_or(rtype::LoginAccepted{}).session);
    REQUIRE(history != nullptr);
    const rtype::Snapshot* const acknowledged = history->latest_acknowledged();
    REQUIRE(acknowledged != nullptr);
    const rtype::Snapshot* const rebuilt = client.snapshots().find(acknowledged->tick);
    REQUIRE(rebuilt != nullptr);
    CHECK(rebuilt->entities.size() == acknowledged->entities.size());
    for (const rtype::EntityRecord& entity : acknowledged->entities) {
        CHECK(std::ranges::find(rebuilt->entities, rtype::quantized(entity)) !=
              rebuilt->entities.end());
    }
    CHECK(client.snapshots().dropped() == 0);
    const std::optional<aegis2d::ecs::Entity> ship = instance.ship(0);
    REQUIRE(ship.has_value());
    const auto& transform = instance.game().registry().get<aegis2d::ecs::Transform>(
        ship.value_or(aegis2d::ecs::Entity{}));
    CHECK(transform.position.x > rtype::player_spawn(0).x + 100.0F);
    CHECK(transform.position.y == rtype::player_spawn(0).y);
}

TEST_CASE("a client creates and joins rooms from the room list screen", "[client][rooms]") {
    NetworkContext context;
    rtype::GameServer game_server{
        context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0, rtype::tuning::tick_rate, 2};
    ServerNetwork& server = game_server.network();
    ClientSettings settings = settings_for(server, "R9");
    settings.choose_room = true;
    ClientSession client{context, settings};
    client.start();

    // Logged in, the client stays in the lobby with the rooms: the one of its login.
    REQUIRE(pump(context, &server, {&client}, [&] { return !client.rooms().empty(); }));
    CHECK(client.state() == ClientState::Lobby);
    CHECK(client.connects_sent() == 0);
    rtype::RoomListForm screen;
    screen.update(client.rooms());
    REQUIRE(screen.rooms().size() == 1);
    CHECK(screen.rooms().front().name == "Room 1");

    // A refused room says why; the screen creates one of 2 players, listed at once.
    client.create_room({.name = "", .max_players = 2});
    REQUIRE(pump(context, &server, {&client}, [&] { return !client.room_error().empty(); }));
    CHECK(client.room_error() ==
          "the room's name is not valid: 1 to 16 characters, no control character");
    REQUIRE(screen.creating());
    for (const char character : std::string{"Aces"}) {
        static_cast<void>(screen.type(character));
    }
    screen.smaller();
    screen.smaller();
    client.create_room(std::get<rtype::CreateRoomRequest>(screen.submit()));
    REQUIRE(pump(context, &server, {&client}, [&] { return client.rooms().size() == 2; }));
    CHECK(client.room_error().empty());
    screen.update(client.rooms());
    CHECK(screen.rooms().at(1).name == "Aces");
    CHECK(screen.rooms().at(1).max_players == 2);

    // Joined from the screen, the client is the first player of that room, alone there: ready,
    // it plays in it.
    screen.down();
    screen.down();
    client.join_room(std::get<std::uint8_t>(screen.submit()));
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InRoom; }));
    client.set_ready(true);
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    const rtype::LoginAccepted login = client.login().value_or(rtype::LoginAccepted{});
    CHECK(login.player == 0);
    const rtype::Session* const session = server.sessions().find(login.session);
    REQUIRE(session != nullptr);
    CHECK(session->room == 1);
    CHECK(game_server.open_rooms() == 1);
    CHECK_NOTHROW(game_server.room(1));
    CHECK(server.sessions().list_rooms().size() == 1);
}

TEST_CASE("four clients fill and start a room, while a second room plays", "[client][rooms]") {
    NetworkContext context;
    rtype::GameServer game_server{
        context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0, rtype::tuning::tick_rate, 3};
    ServerNetwork& server = game_server.network();
    // A player who did not choose its room plays in the first one.
    ClientSession playing{context, settings_for(server, "R0")};
    playing.start();
    std::vector<std::unique_ptr<ClientSession>> clients;
    clients.reserve(4);
    for (int index = 1; index <= 4; ++index) {
        ClientSettings settings = settings_for(server, "p" + std::to_string(index));
        settings.choose_room = true;
        clients.push_back(std::make_unique<ClientSession>(context, settings));
        clients.back()->start();
    }
    const auto run_until = [&](auto condition) {
        return pump(context, &server,
                    {&playing, clients.at(0).get(), clients.at(1).get(), clients.at(2).get(),
                     clients.at(3).get()},
                    condition);
    };
    const auto all_in = [&](ClientState state) {
        return std::ranges::all_of(clients,
                                   [&](const auto& client) { return client->state() == state; });
    };
    REQUIRE(run_until(
        [&] { return playing.state() == ClientState::InGame && all_in(ClientState::Lobby); }));

    // The first one creates a room; the four join it and see each other there.
    clients.at(0)->create_room({.name = "Aces", .max_players = 4});
    const auto aces = [&]() -> std::optional<std::uint8_t> {
        for (const rtype::RoomRecord& room : clients.at(0)->rooms()) {
            if (room.name == "Aces") {
                return room.room;
            }
        }
        return std::nullopt;
    };
    REQUIRE(run_until([&] { return aces().has_value(); }));
    const std::uint8_t room = aces().value_or(0);
    for (const auto& client : clients) {
        client->join_room(room);
    }
    REQUIRE(run_until([&] {
        return all_in(ClientState::InRoom) && std::ranges::all_of(clients, [](const auto& client) {
                   return client->room_players().players.size() == 4;
               });
    }));

    // One leaves and comes back; three get ready: they all see it, nothing starts.
    clients.at(3)->leave_room();
    CHECK(clients.at(3)->state() == ClientState::Lobby);
    REQUIRE(run_until([&] { return clients.at(0)->room_players().players.size() == 3; }));
    clients.at(3)->join_room(room);
    REQUIRE(run_until([&] { return clients.at(0)->room_players().players.size() == 4; }));
    for (std::size_t index = 0; index < 3; ++index) {
        clients.at(index)->set_ready(true);
    }
    REQUIRE(run_until([&] {
        return std::ranges::count_if(
                   clients.at(3)->room_players().players,
                   [](const rtype::RoomPlayer& player) { return player.ready; }) == 3;
    }));
    CHECK(all_in(ClientState::InRoom));

    // The fourth gets ready: the game of the room starts for the four, beside the first room's.
    clients.at(3)->set_ready(true);
    REQUIRE(run_until([&] { return all_in(ClientState::InGame); }));
    std::vector<std::uint8_t> numbers;
    for (const auto& client : clients) {
        const rtype::LoginAccepted login = client->login().value_or(rtype::LoginAccepted{});
        const rtype::Session* const session = server.sessions().find(login.session);
        REQUIRE(session != nullptr);
        CHECK(session->room == room);
        numbers.push_back(login.player);
    }
    std::ranges::sort(numbers);
    CHECK(numbers == std::vector<std::uint8_t>{0, 1, 2, 3});
    CHECK(game_server.open_rooms() == 2);
    CHECK(playing.state() == ClientState::InGame);
    const rtype::Session* const first =
        server.sessions().find(playing.login().value_or(rtype::LoginAccepted{}).session);
    REQUIRE(first != nullptr);
    CHECK(first->room != room);
}
