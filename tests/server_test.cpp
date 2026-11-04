#include <aegis2d/core/random.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/binary.hpp>
#include <aegis2d/net/compression.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/reliability.hpp>
#include <aegis2d/net/socket_error.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/delta_snapshot.hpp>
#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/game_server.hpp>
#include <rtype/login.hpp>
#include <rtype/rate_limit.hpp>
#include <rtype/server_network.hpp>
#include <rtype/server_options.hpp>
#include <rtype/sessions.hpp>
#include <rtype/snapshot.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

using aegis2d::net::Endpoint;
using aegis2d::net::NetworkContext;
using aegis2d::net::PacketHeader;
using aegis2d::net::PacketType;
using aegis2d::net::SocketError;
using aegis2d::net::TcpCloseReason;
using aegis2d::net::TcpConnection;
using aegis2d::net::TcpListener;
using aegis2d::net::UdpSocket;
using rtype::LoginAccepted;
using rtype::LoginRejection;
using rtype::LoginRequest;
using rtype::LogoutReason;
using rtype::ServerCommand;
using rtype::ServerNetwork;
using rtype::ServerOptions;

namespace {

std::variant<ServerOptions, std::string> parse_args(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> list{args};
    return rtype::parse_server_options(list);
}

ServerOptions parse(std::initializer_list<std::string_view> args) {
    const std::variant<ServerOptions, std::string> result = parse_args(args);
    REQUIRE(std::holds_alternative<ServerOptions>(result));
    return std::get<ServerOptions>(result);
}

std::string error_of(std::initializer_list<std::string_view> args) {
    const std::variant<ServerOptions, std::string> result = parse_args(args);
    REQUIRE(std::holds_alternative<std::string>(result));
    return std::get<std::string>(result);
}

// Longest wait of a test for something that takes milliseconds: generous, as a loaded CI runner
// can be seconds late, and a test that fails must fail on its assertion, not on a timer.
constexpr std::chrono::seconds test_deadline{10};

// Runs the context until the condition holds, for at most test_deadline.
template <typename Condition>
bool run_until(NetworkContext& context, Condition condition) {
    const auto deadline = std::chrono::steady_clock::now() + test_deadline;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        context.run_for(std::chrono::milliseconds{10});
    }
    return true;
}

// Runs the context and the server's update() until the condition holds, for at most
// test_deadline, the server's clock being shifted by offset.
template <typename Condition>
bool pump(NetworkContext& context, ServerNetwork& server, Condition condition,
          ServerNetwork::Clock::duration offset = {}) {
    const auto deadline = std::chrono::steady_clock::now() + test_deadline;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        context.run_for(std::chrono::milliseconds{10});
        server.update(ServerNetwork::Clock::now() + offset);
    }
    return true;
}

// A player's TCP connection, keeping what the server sends and whether it closed.
struct Player {
    TcpConnection connection;
    std::vector<std::vector<std::byte>> received;
    bool closed = false;
    // A member, not a local: the handler must not point into a constructor that failed.
    std::optional<std::error_code> connected;

    Player(NetworkContext& context, ServerNetwork& server) : connection{context} {
        connection.connect(Endpoint::loopback(server.tcp_endpoint().port()),
                           [this](const std::error_code& error) { connected = error; });
        REQUIRE(pump(context, server, [&] { return connected.has_value(); }));
        REQUIRE(connected == std::error_code{});
        connection.start(
            [this](std::span<const std::byte> message) {
                received.emplace_back(message.begin(), message.end());
            },
            [this](TcpCloseReason /*reason*/) { closed = true; });
    }

    void send(const std::vector<std::byte>& message) { connection.send(message); }
};

// A datagram of the given type and token, with an optional payload.
std::vector<std::byte> datagram(PacketType type, std::uint32_t session, std::uint16_t sequence = 0,
                                std::vector<std::byte> payload = {}) {
    aegis2d::net::BinaryWriter writer;
    aegis2d::net::write_header(
        writer,
        PacketHeader{
            .type = type, .sequence = sequence, .ack = 65535, .ack_bits = 0, .session = session});
    writer.write_bytes(payload);
    return {writer.data().begin(), writer.data().end()};
}

// The payload of an Input (RFC section 4.2), written by hand so it can be malformed: its
// sequence, little-endian, then its actions.
std::vector<std::byte> input_payload(std::uint16_t sequence, std::uint8_t actions) {
    return {std::byte{static_cast<std::uint8_t>(sequence & 0xFFU)},
            std::byte{static_cast<std::uint8_t>(sequence >> 8U)}, std::byte{actions}};
}

// A player's UDP socket, keeping the headers of the datagrams the server sends, and the
// datagrams themselves.
struct Socket {
    UdpSocket socket;
    std::vector<PacketHeader> received;
    std::vector<std::size_t> sizes;
    std::vector<std::vector<std::byte>> datagrams;

    explicit Socket(NetworkContext& context) : socket{context, Endpoint::loopback(0)} {
        socket.start_receiving([this](const Endpoint& /*from*/, std::span<const std::byte> bytes) {
            aegis2d::net::BinaryReader reader{bytes};
            const auto header = aegis2d::net::read_header(reader);
            if (const auto* const read = std::get_if<PacketHeader>(&header)) {
                received.push_back(*read);
                sizes.push_back(bytes.size());
                datagrams.emplace_back(bytes.begin(), bytes.end());
            }
        });
    }

    void send(ServerNetwork& server, const std::vector<std::byte>& bytes) {
        socket.send_to(Endpoint::loopback(server.udp_endpoint().port()), bytes);
    }
};

LoginRequest request(std::string name) {
    return LoginRequest{.version = rtype::protocol_version, .name = std::move(name)};
}

} // namespace

TEST_CASE("without arguments the server runs on the default ports", "[server][options]") {
    const ServerOptions options = parse({});
    CHECK(options.command == ServerCommand::Run);
    CHECK(options.tcp_port == rtype::default_tcp_port);
    CHECK(options.udp_port == rtype::default_udp_port);
    CHECK(!options.waves.has_value());
}

TEST_CASE("ports and waves file are read in any order", "[server][options]") {
    const ServerOptions options =
        parse({"--udp-port", "5000", "--waves", "stage2.json", "--tcp-port", "65535"});
    CHECK(options.command == ServerCommand::Run);
    CHECK(options.tcp_port == 65535);
    CHECK(options.udp_port == 5000);
    CHECK(options.waves == std::filesystem::path{"stage2.json"});

    CHECK(parse({"--tcp-port", "0"}).tcp_port == 0);
    CHECK(parse({"--tcp-port", "1", "--tcp-port", "2"}).tcp_port == 2);
}

TEST_CASE("the packet rate limit is read from the command line", "[server][options]") {
    CHECK(parse({}).packet_rate == rtype::default_packet_rate);
    CHECK(parse({"--max-packet-rate", "1000"}).packet_rate == 1000);
    CHECK(parse({"--max-packet-rate", "100000"}).packet_rate == 100000);
    for (const std::string_view rate : {"0", "100001", "-5", "fast", ""}) {
        CAPTURE(rate);
        CHECK(error_of({"--max-packet-rate", rate}) ==
              "--max-packet-rate: \"" + std::string{rate} + "\" is not a rate (1 to 100000)");
    }
    CHECK(error_of({"--max-packet-rate"}) == "--max-packet-rate needs a value");
}

TEST_CASE("the compression threshold is read from the command line", "[server][options]") {
    CHECK(!parse({}).compress_from.has_value());
    CHECK(parse({"--compress", "128"}).compress_from == 128U);
    CHECK(parse({"--compress", "1"}).compress_from == 1U);
    CHECK(parse({"--compress", "1182"}).compress_from == 1182U);
    for (const std::string_view size : {"0", "1183", "-5", "big", ""}) {
        CAPTURE(size);
        CHECK(error_of({"--compress", size}) ==
              "--compress: \"" + std::string{size} + "\" is not a payload size (1 to 1182)");
    }
    CHECK(error_of({"--compress"}) == "--compress needs a value");
}

TEST_CASE("help and version stop the reading", "[server][options]") {
    CHECK(parse({"--help"}).command == ServerCommand::Help);
    CHECK(parse({"--version"}).command == ServerCommand::Version);
    CHECK(parse({"--tcp-port", "1", "--version", "--bad"}).command == ServerCommand::Version);
}

TEST_CASE("bad command lines name the faulty argument", "[server][options]") {
    CHECK(error_of({"--port"}) == "unknown argument \"--port\"");
    CHECK(error_of({"4242"}) == "unknown argument \"4242\"");
    CHECK(error_of({"--tcp-port"}) == "--tcp-port needs a value");
    CHECK(error_of({"--waves"}) == "--waves needs a value");
    for (const std::string_view port : {"", "65536", "-1", "+1", "42abc", "0x10", " 42", "abc"}) {
        CAPTURE(port);
        CHECK(error_of({"--udp-port", port}) ==
              "--udp-port: \"" + std::string{port} + "\" is not a port (0 to 65535)");
    }
}

TEST_CASE("the server listens on the given ports", "[server][network]") {
    NetworkContext context;
    // Ports picked by the system, then asked for explicitly, as the command line does.
    std::uint16_t tcp_port = 0;
    std::uint16_t udp_port = 0;
    {
        const ServerNetwork probe{context, 0, 0};
        tcp_port = probe.tcp_endpoint().port();
        udp_port = probe.udp_endpoint().port();
    }
    REQUIRE(tcp_port != 0);
    REQUIRE(udp_port != 0);
    const ServerNetwork server{context, tcp_port, udp_port};
    CHECK(server.tcp_endpoint().port() == tcp_port);
    CHECK(server.udp_endpoint().port() == udp_port);

    TcpConnection client{context};
    std::optional<std::error_code> connected;
    client.connect(Endpoint::loopback(tcp_port),
                   [&](const std::error_code& error) { connected = error; });
    REQUIRE(run_until(context, [&] { return server.connections() == 1; }));
    CHECK(connected == std::error_code{});

    UdpSocket player{context, 0};
    const std::array datagram{std::byte{0x52}, std::byte{0x54}, std::byte{0x59}, std::byte{0x50}};
    player.send_to(Endpoint::loopback(udp_port), datagram);
    REQUIRE(run_until(context, [&] { return server.datagrams() == 1; }));
}

TEST_CASE("a port already in use stops the server", "[server][network]") {
    NetworkContext context;
    const TcpListener taken{context, 0};
    CHECK_THROWS_AS((ServerNetwork{context, taken.local_endpoint().port(), 0}), SocketError);

    const UdpSocket busy{context, 0};
    CHECK_THROWS_AS((ServerNetwork{context, 0, busy.local_endpoint().port()}), SocketError);
}

TEST_CASE("a client logs in and receives a token", "[server][login]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0, rtype::SessionTable{5}};
    Player player{context, server};
    player.send(rtype::write_login_request(request("R9")));
    REQUIRE(pump(context, server, [&] { return !player.received.empty(); }));

    REQUIRE(server.sessions().size() == 1);
    REQUIRE(player.received.size() == 1);
    const std::vector<std::byte>& reply = player.received.front();
    REQUIRE(reply.size() == 8);
    CHECK(rtype::kind_of(reply) == rtype::TcpKind::LoginAccepted);
    const std::uint32_t token = std::to_integer<std::uint32_t>(reply.at(1)) |
                                (std::to_integer<std::uint32_t>(reply.at(2)) << 8U) |
                                (std::to_integer<std::uint32_t>(reply.at(3)) << 16U) |
                                (std::to_integer<std::uint32_t>(reply.at(4)) << 24U);
    const rtype::Session* const session = server.sessions().find(token);
    REQUIRE(session != nullptr);
    CHECK(session->name == "R9");
    CHECK(reply == rtype::write_login_accepted(LoginAccepted{
                       .session = token, .player = 0, .udp_port = server.udp_endpoint().port()}));
    CHECK(!player.closed);

    // The session lasts while the connection is open, and ends with a Logout.
    context.run_for(std::chrono::milliseconds{50});
    server.update(ServerNetwork::Clock::now() + ServerNetwork::login_timeout * 2);
    CHECK(server.sessions().size() == 1);
    player.send(rtype::write_logout(LogoutReason::Left));
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == 0; }));
    REQUIRE(pump(context, server, [&] { return player.closed; }));
}

TEST_CASE("a refused login gets its reason, then the connection closes", "[server][login]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    Player first{context, server};
    first.send(rtype::write_login_request(request("R9")));
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == 1; }));

    Player second{context, server};
    second.send(rtype::write_login_request(request("R9")));
    REQUIRE(pump(context, server, [&] { return second.closed; }));
    REQUIRE(second.received.size() == 1);
    CHECK(second.received.front() == rtype::write_login_rejected(LoginRejection::NameTaken));

    Player old{context, server};
    old.send(rtype::write_login_request(LoginRequest{.version = 2, .name = "old"}));
    REQUIRE(pump(context, server, [&] { return old.closed; }));
    REQUIRE(old.received.size() == 1);
    CHECK(old.received.front() == rtype::write_login_rejected(LoginRejection::BadVersion));
    CHECK(server.sessions().size() == 1);
}

TEST_CASE("the fifth player gets a full-server rejection", "[server][login]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<std::unique_ptr<Player>> players;
    for (int index = 0; index < 4; ++index) {
        players.push_back(std::make_unique<Player>(context, server));
        players.back()->send(rtype::write_login_request(request("p" + std::to_string(index))));
    }
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == 4; }));

    Player fifth{context, server};
    fifth.send(rtype::write_login_request(request("p4")));
    REQUIRE(pump(context, server, [&] { return fifth.closed; }));
    REQUIRE(fifth.received.size() == 1);
    CHECK(fifth.received.front() == rtype::write_login_rejected(LoginRejection::ServerFull));

    // A player leaving by closing its connection frees its slot.
    players.front()->connection.close();
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == 3; }));
}

TEST_CASE("bad or missing logins are closed without a reply", "[server][login]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};

    Player garbage{context, server};
    garbage.send(std::vector<std::byte>{std::byte{0x09}, std::byte{0x00}});
    Player empty{context, server};
    empty.send({});
    Player truncated{context, server};
    truncated.send(std::vector<std::byte>{std::byte{0x01}, std::byte{0x01}, std::byte{0x05}});
    Player logout_first{context, server};
    logout_first.send(rtype::write_logout(LogoutReason::Left));
    REQUIRE(pump(context, server, [&] {
        return garbage.closed && empty.closed && truncated.closed && logout_first.closed;
    }));
    for (const Player* const player : {&garbage, &empty, &truncated, &logout_first}) {
        CHECK(player->received.empty());
    }

    // Silent for login_timeout: closed. Logged-in players are not affected.
    Player logged{context, server};
    logged.send(rtype::write_login_request(request("R9")));
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == 1; }));
    Player silent{context, server};
    REQUIRE(pump(context, server, [&] { return silent.closed; }, ServerNetwork::login_timeout));
    CHECK(silent.received.empty());
    CHECK(!logged.closed);
    CHECK(server.sessions().size() == 1);
    REQUIRE(pump(context, server, [&] { return server.open_connections() == 1; }));
}

TEST_CASE("a logged-in player sending anything but a Logout loses its session", "[server][login]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    Player player{context, server};
    player.send(rtype::write_login_request(request("R9")));
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == 1; }));
    player.send(rtype::write_login_request(request("R9")));
    REQUIRE(pump(context, server, [&] { return player.closed; }));
    CHECK(server.sessions().size() == 0);
    CHECK(player.received.size() == 1);
}

namespace {

// Logs a player in over TCP and returns its token.
std::uint32_t log_in(NetworkContext& context, ServerNetwork& server, Player& player,
                     std::string name) {
    const std::size_t before = server.sessions().size();
    player.send(rtype::write_login_request(request(std::move(name))));
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == before + 1; }));
    REQUIRE(pump(context, server, [&] { return !player.received.empty(); }));
    const std::vector<std::byte>& reply = player.received.back();
    REQUIRE(reply.size() == 8);
    return std::to_integer<std::uint32_t>(reply.at(1)) |
           (std::to_integer<std::uint32_t>(reply.at(2)) << 8U) |
           (std::to_integer<std::uint32_t>(reply.at(3)) << 16U) |
           (std::to_integer<std::uint32_t>(reply.at(4)) << 24U);
}

} // namespace

TEST_CASE("a Connect binds the endpoint and gets an Accept", "[server][udp]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    Player player{context, server};
    const std::uint32_t token = log_in(context, server, player, "R9");
    Socket udp{context};

    udp.send(server, datagram(PacketType::Connect, token, 0));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 1; }));
    const PacketHeader& accept = udp.received.front();
    CHECK(accept.type == PacketType::Accept);
    CHECK(accept.session == token);
    CHECK(accept.sequence == 0);
    CHECK(accept.ack == 0); // the Connect of sequence 0
    CHECK(udp.sizes.front() == PacketHeader::size);
    REQUIRE(server.sessions().find(token) != nullptr);
    CHECK(server.sessions().find(token)->endpoint == udp.socket.local_endpoint());

    // The Accept was lost: the next Connect gets another one, numbered after it.
    udp.send(server, datagram(PacketType::Connect, token, 1));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 2; }));
    CHECK(udp.received.back().type == PacketType::Accept);
    CHECK(udp.received.back().sequence == 1);
    CHECK(udp.received.back().ack == 1);
    CHECK(udp.received.back().ack_bits == 1U);

    // Bound: its other datagrams reach the game.
    udp.send(server, datagram(PacketType::Heartbeat, token, 2));
    REQUIRE(pump(context, server, [&] { return server.game_datagrams() == 1; }));
    CHECK(server.dropped_datagrams() == 0);
}

TEST_CASE("datagrams from unknown endpoints are dropped without an answer", "[server][udp]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    Player player{context, server};
    const std::uint32_t token = log_in(context, server, player, "R9");
    Socket owner{context};
    owner.send(server, datagram(PacketType::Connect, token));
    REQUIRE(pump(context, server, [&] { return owner.received.size() == 1; }));

    Socket stranger{context};
    const std::vector<std::vector<std::byte>> bad = {
        datagram(PacketType::Input, token, 0, input_payload(0, 0x01)), // endpoint not bound
        datagram(PacketType::Connect, token),                          // session bound elsewhere
        datagram(PacketType::Connect, token + 1),                      // unknown session
        datagram(PacketType::Connect, 0),                              // 0 is never a token
        datagram(PacketType::Snapshot, token),                         // wrong direction
        {std::byte{0x52}, std::byte{0x54}, std::byte{0x59}},           // truncated header
    };
    for (const std::vector<std::byte>& bytes : bad) {
        stranger.send(server, bytes);
    }
    // From the bound endpoint: another session's token, and a Connect with a payload.
    owner.send(server, datagram(PacketType::Heartbeat, token + 1, 1));
    owner.send(server, datagram(PacketType::Connect, token, 2, {std::byte{0x00}}));
    const std::uint64_t expected = bad.size() + 2;
    REQUIRE(pump(context, server, [&] { return server.dropped_datagrams() == expected; }));

    context.run_for(std::chrono::milliseconds{50});
    CHECK(stranger.received.empty());
    CHECK(owner.received.size() == 1);
    CHECK(server.game_datagrams() == 0);
    CHECK(server.sessions().find(token)->endpoint == owner.socket.local_endpoint());
}

TEST_CASE("ending a session unbinds its endpoint", "[server][udp]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    Player player{context, server};
    const std::uint32_t token = log_in(context, server, player, "R9");
    Socket udp{context};
    udp.send(server, datagram(PacketType::Connect, token));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 1; }));

    player.send(rtype::write_logout(LogoutReason::Left));
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == 0; }));
    udp.send(server, datagram(PacketType::Heartbeat, token, 1));
    udp.send(server, datagram(PacketType::Connect, token, 2));
    REQUIRE(pump(context, server, [&] { return server.dropped_datagrams() == 2; }));
    CHECK(udp.received.size() == 1);
    CHECK(server.game_datagrams() == 0);
}

TEST_CASE("players join the game when bound and leave when their session ends", "[server][game]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<unsigned> joined;
    std::vector<unsigned> left;
    server.on_players([&](const rtype::Session& session) { joined.push_back(session.player); },
                      [&](const rtype::Session& session, rtype::LeaveReason /*reason*/) {
                          left.push_back(session.player);
                      });

    // Four players log in and bind their endpoints: each joins once, with its number.
    std::vector<std::unique_ptr<Player>> players;
    std::vector<std::unique_ptr<Socket>> sockets;
    std::vector<std::uint32_t> tokens;
    for (int index = 0; index < 4; ++index) {
        players.push_back(std::make_unique<Player>(context, server));
        tokens.push_back(log_in(context, server, *players.back(), "p" + std::to_string(index)));
        sockets.push_back(std::make_unique<Socket>(context));
        sockets.back()->send(server, datagram(PacketType::Connect, tokens.back()));
        sockets.back()->send(server, datagram(PacketType::Connect, tokens.back(), 1));
    }
    REQUIRE(pump(context, server, [&] { return joined.size() == 4; }));
    REQUIRE(pump(context, server, [&] { return sockets.back()->received.size() == 2; }));
    CHECK(joined == std::vector<unsigned>{0, 1, 2, 3});

    // The fifth login gets a full-game rejection.
    Player fifth{context, server};
    fifth.send(rtype::write_login_request(request("p4")));
    REQUIRE(pump(context, server, [&] { return fifth.closed; }));
    REQUIRE(fifth.received.size() == 1);
    CHECK(fifth.received.front() == rtype::write_login_rejected(LoginRejection::ServerFull));

    // Player 1 leaves; a login that never binds its endpoint never joins.
    players.at(1)->send(rtype::write_logout(LogoutReason::Left));
    REQUIRE(pump(context, server, [&] { return left.size() == 1; }));
    CHECK(left.front() == 1);
    Player late{context, server};
    static_cast<void>(log_in(context, server, late, "late"));
    late.connection.close();
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == 3; }));
    CHECK(joined.size() == 4);
    CHECK(left.size() == 1);
}

TEST_CASE("the most recent valid input of a bound player reaches the game", "[server][input]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<std::uint8_t> inputs;
    std::vector<std::uint16_t> sequences;
    std::vector<unsigned> left;
    server.on_input([&](const rtype::Session& /*session*/, aegis2d::ecs::PlayerInput input,
                        std::uint16_t sequence) {
        inputs.push_back(input.held);
        sequences.push_back(sequence);
    });
    server.on_players([](const rtype::Session& /*session*/) {},
                      [&](const rtype::Session& session, rtype::LeaveReason /*reason*/) {
                          left.push_back(session.player);
                      });
    Player player{context, server};
    const std::uint32_t token = log_in(context, server, player, "R9");
    Socket udp{context};
    udp.send(server, datagram(PacketType::Connect, token, 0));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 1; }));

    // The datagram's sequence, then the payload: the input's own sequence and its actions.
    const auto input = [&](std::uint16_t sequence, std::vector<std::byte> payload) {
        udp.send(server, datagram(PacketType::Input, token, sequence, std::move(payload)));
    };
    std::vector<std::byte> too_long = input_payload(14, 0x08);
    too_long.push_back(std::byte{0});
    input(1, input_payload(10, 0x08));       // right: applied
    input(2, input_payload(11, 0x20));       // bit 5 set: malformed
    input(3, {std::byte{11}, std::byte{0}}); // no actions: malformed
    input(4, std::move(too_long));           // a byte too many: malformed
    input(5, input_payload(13, 0x01));       // up: applied
    input(6, input_payload(12, 0x02));       // an older input in a newer datagram: ignored
    input(5, input_payload(13, 0x01));       // duplicate: ignored
    input(7, input_payload(13, 0x04));       // the same input again: ignored
    REQUIRE(pump(context, server, [&] { return server.dropped_datagrams() == 3; }));
    REQUIRE(pump(context, server, [&] { return server.game_datagrams() == 5; }));
    CHECK(inputs == std::vector<std::uint8_t>{0x08, 0x01});
    CHECK(sequences == std::vector<std::uint16_t>{10, 13});

    // The next datagram to the player acknowledges 8, 7, 6, 5, 1 and 0, not the malformed 2
    // to 4.
    udp.send(server, datagram(PacketType::Connect, token, 8));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 2; }));
    CHECK(udp.received.back().ack == 8);
    CHECK(udp.received.back().ack_bits == 0b11000111U);

    // Input sequences wrap from 65535 to 0, as datagram sequences do (RFC section 2.2).
    input(9, input_payload(30000, 0x08));
    input(10, input_payload(60000, 0x08));
    input(11, input_payload(65535, 0x08));
    input(12, input_payload(0, 0x10));
    input(13, input_payload(65534, 0x01)); // older than 0: ignored
    REQUIRE(pump(context, server, [&] { return server.game_datagrams() == 10; }));
    CHECK(sequences == std::vector<std::uint16_t>{10, 13, 30000, 60000, 65535, 0});
    CHECK(inputs.back() == 0x10);
    CHECK(left.empty());
}

TEST_CASE("heartbeats, events and disconnects of a bound player are checked", "[server][input]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<unsigned> left;
    server.on_players([](const rtype::Session& /*session*/) {},
                      [&](const rtype::Session& session, rtype::LeaveReason /*reason*/) {
                          left.push_back(session.player);
                      });
    Player player{context, server};
    const std::uint32_t token = log_in(context, server, player, "R9");
    Socket udp{context};
    udp.send(server, datagram(PacketType::Connect, token, 0));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 1; }));

    udp.send(server, datagram(PacketType::Heartbeat, token, 1));
    udp.send(server, datagram(PacketType::Heartbeat, token, 2, {std::byte{0}}));
    // An Event with one empty event (id 0, length 0), then one announcing no event.
    udp.send(server,
             datagram(PacketType::Event, token, 3,
                      {std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}}));
    udp.send(server, datagram(PacketType::Event, token, 4, {std::byte{0}}));
    udp.send(server, datagram(PacketType::Disconnect, token, 5, {std::byte{0}}));
    REQUIRE(pump(context, server, [&] { return server.dropped_datagrams() == 3; }));
    CHECK(server.game_datagrams() == 2);
    CHECK(server.sessions().size() == 1);

    // A Disconnect ends the session and closes its TCP connection.
    udp.send(server, datagram(PacketType::Disconnect, token, 6));
    REQUIRE(pump(context, server, [&] { return player.closed; }));
    CHECK(server.sessions().size() == 0);
    CHECK(left == std::vector<unsigned>{0});
}

TEST_CASE("snapshots go to bound players, numbered and acknowledging their datagrams",
          "[server][snapshot]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    Player bound_player{context, server};
    const std::uint32_t token = log_in(context, server, bound_player, "R9");
    Player unbound_player{context, server};
    static_cast<void>(log_in(context, server, unbound_player, "R10"));
    Socket udp{context};
    udp.send(server, datagram(PacketType::Connect, token, 0));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 1; }));
    udp.send(server, datagram(PacketType::Input, token, 1, input_payload(0, 0x08)));
    REQUIRE(pump(context, server, [&] { return server.game_datagrams() == 1; }));

    rtype::Snapshot snapshot{.tick = 9, .players = {}, .entities = {}};
    snapshot.entities.resize(130);
    server.send_snapshot(rtype::write_snapshot(snapshot));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 3; }));

    // Two datagrams (122 + 8 entities), after the Accept of sequence 0, acknowledging 1 and 0;
    // only the bound player gets them.
    CHECK(server.snapshots_sent() == 2);
    for (std::size_t index = 1; index < 3; ++index) {
        const PacketHeader& header = udp.received.at(index);
        CHECK(header.type == PacketType::Snapshot);
        CHECK(header.session == token);
        CHECK(header.sequence == index);
        CHECK(header.ack == 1);
        CHECK(header.ack_bits == 1U);
    }
    // 51 bits, then 77 bits per record, rounded up to the byte.
    CHECK(udp.sizes.at(1) == PacketHeader::size + ((51 + (std::size_t{122} * 77) + 7) / 8));
    CHECK(udp.sizes.at(2) == PacketHeader::size + ((51 + (std::size_t{8} * 77) + 7) / 8));
}

TEST_CASE("payloads from the compression threshold go compressed, and back whole",
          "[server][snapshot][compression]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    Player player{context, server};
    const std::uint32_t token = log_in(context, server, player, "R9");
    Socket udp{context};
    udp.send(server, datagram(PacketType::Connect, token, 0));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 1; }));

    // 130 still entities in two payloads, sent plain, then compressed from 128 bytes.
    rtype::Snapshot snapshot{.tick = 9, .players = {}, .entities = {}};
    snapshot.entities.resize(130);
    const std::vector<std::vector<std::byte>> payloads = rtype::write_snapshot(snapshot);
    REQUIRE(payloads.size() == 2);
    CHECK(!server.compression().has_value());
    server.send_snapshot(payloads);
    server.set_compression(128);
    CHECK(server.compression() == 128U);
    server.send_snapshot(payloads);
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 5; }));

    for (std::size_t index = 0; index < payloads.size(); ++index) {
        CAPTURE(index);
        const std::vector<std::byte>& plain = udp.datagrams.at(1 + index);
        const std::vector<std::byte>& packed = udp.datagrams.at(3 + index);
        CHECK(!udp.received.at(1 + index).compressed);
        // The second payload, 8 records, is below the threshold.
        CHECK(udp.received.at(3 + index).compressed == (index == 0));
        CHECK((packed.size() < plain.size()) == (index == 0));
        aegis2d::net::BinaryReader reader{packed};
        REQUIRE(std::holds_alternative<PacketHeader>(aegis2d::net::read_header(reader)));
        CHECK(aegis2d::net::read_payload(udp.received.at(3 + index), reader) == payloads.at(index));
    }

    // A compressed payload that does not decompress is dropped before anything reads it.
    std::vector<std::byte> broken = datagram(PacketType::Heartbeat, token, 1, {std::byte{0x10}});
    broken.at(5) |= std::byte{PacketHeader::compressed_flag};
    const std::uint64_t dropped = server.dropped_datagrams();
    udp.send(server, broken);
    REQUIRE(pump(context, server, [&] { return server.dropped_datagrams() == dropped + 1; }));
    CHECK(server.game_datagrams() == 0);
}

TEST_CASE("the server knows the latest snapshot each player acknowledged",
          "[server][snapshot][baselines]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    Player first_player{context, server};
    const std::uint32_t first = log_in(context, server, first_player, "R9");
    Player second_player{context, server};
    const std::uint32_t second = log_in(context, server, second_player, "R10");
    Socket first_udp{context};
    Socket second_udp{context};
    first_udp.send(server, datagram(PacketType::Connect, first, 0));
    second_udp.send(server, datagram(PacketType::Connect, second, 0));
    REQUIRE(pump(context, server, [&] {
        return first_udp.received.size() == 1 && second_udp.received.size() == 1;
    }));
    REQUIRE(server.snapshots(first) != nullptr);
    CHECK(server.snapshots(first)->latest_acknowledged() == nullptr);
    CHECK(server.snapshots(first + second + 1) == nullptr);

    // Ticks 3 and 6, one datagram each: sequences 1 and 2 of both channels, after the Accept.
    for (const std::uint32_t tick : {3U, 6U}) {
        const auto snapshot = std::make_shared<const rtype::Snapshot>(
            rtype::Snapshot{.tick = tick, .players = {}, .entities = {}});
        server.send_snapshot(rtype::write_snapshot(*snapshot), snapshot);
    }
    REQUIRE(pump(context, server, [&] {
        return first_udp.received.size() == 3 && second_udp.received.size() == 3;
    }));
    CHECK(server.snapshots(first)->size() == 2);

    // The first player received both, the second only the first one (sequence 2 lost).
    const auto heartbeat = [](std::uint32_t session, std::uint16_t ack, std::uint32_t bits) {
        aegis2d::net::BinaryWriter writer;
        aegis2d::net::write_header(writer, PacketHeader{.type = PacketType::Heartbeat,
                                                        .sequence = 1,
                                                        .ack = ack,
                                                        .ack_bits = bits,
                                                        .session = session});
        return std::vector<std::byte>{writer.data().begin(), writer.data().end()};
    };
    first_udp.send(server, heartbeat(first, 2, 0b11U));
    second_udp.send(server, heartbeat(second, 1, 0b1U));
    REQUIRE(pump(context, server, [&] {
        const rtype::Snapshot* const one = server.snapshots(first)->latest_acknowledged();
        const rtype::Snapshot* const two = server.snapshots(second)->latest_acknowledged();
        return one != nullptr && two != nullptr;
    }));
    CHECK(server.snapshots(first)->latest_acknowledged()->tick == 6);
    CHECK(server.snapshots(second)->latest_acknowledged()->tick == 3);
    CHECK(!server.snapshots(second)->acknowledged(6));

    // The next snapshot goes to each player against the latest one it acknowledged.
    server.send_snapshot(std::make_shared<const rtype::Snapshot>(
        rtype::Snapshot{.tick = 9, .players = {}, .entities = {}}));
    REQUIRE(pump(context, server, [&] {
        return first_udp.received.size() == 4 && second_udp.received.size() == 4;
    }));
    const auto baseline_of = [](const std::vector<std::byte>& datagram) {
        const std::optional<rtype::DeltaSnapshot> delta =
            rtype::read_delta_snapshot(std::span{datagram}.subspan(PacketHeader::size));
        REQUIRE(delta.has_value());
        return delta.value_or(rtype::DeltaSnapshot{}).baseline;
    };
    CHECK(baseline_of(first_udp.datagrams.back()) == 6U);
    CHECK(baseline_of(second_udp.datagrams.back()) == 3U);
    CHECK(server.snapshots(first)->find(9) != nullptr);
}

namespace {

// The events of an Event datagram, read as a client does with its channel.
std::vector<std::vector<std::byte>> events_of(aegis2d::net::ReliableChannel& channel,
                                              std::span<const std::byte> bytes) {
    aegis2d::net::BinaryReader reader{bytes};
    const auto header = aegis2d::net::read_header(reader);
    const auto* const read = std::get_if<PacketHeader>(&header);
    REQUIRE(read != nullptr);
    REQUIRE(read->type == PacketType::Event);
    std::vector<std::vector<std::byte>> events;
    REQUIRE(channel.read_events(*read, reader, events));
    return events;
}

// A Heartbeat acknowledging the server's datagram of the given sequence and the 32 before.
std::vector<std::byte> acknowledging(std::uint32_t session, std::uint16_t sequence,
                                     std::uint16_t ack) {
    aegis2d::net::BinaryWriter writer;
    aegis2d::net::write_header(writer, PacketHeader{.type = PacketType::Heartbeat,
                                                    .sequence = sequence,
                                                    .ack = ack,
                                                    .ack_bits = 0xFFFFFFFFU,
                                                    .session = session});
    return {writer.data().begin(), writer.data().end()};
}

} // namespace

TEST_CASE("events reach bound players and are resent until acknowledged", "[server][events]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    Player first_player{context, server};
    const std::uint32_t first = log_in(context, server, first_player, "R9");
    Player second_player{context, server};
    const std::uint32_t second = log_in(context, server, second_player, "R10");
    Socket first_udp{context};
    Socket second_udp{context};
    first_udp.send(server, datagram(PacketType::Connect, first, 0));
    second_udp.send(server, datagram(PacketType::Connect, second, 0));
    REQUIRE(pump(context, server, [&] {
        return first_udp.received.size() == 1 && second_udp.received.size() == 1;
    }));

    const std::vector<std::vector<std::byte>> spawns{
        rtype::write_event(rtype::SpawnEvent{.tick = 1, .id = 5, .kind = 3}),
        rtype::write_event(rtype::SpawnEvent{.tick = 1, .id = 6, .kind = 3})};
    server.send_events(spawns);
    REQUIRE(pump(context, server, [&] {
        return first_udp.received.size() == 2 && second_udp.received.size() == 2;
    }));
    CHECK(server.event_datagrams_sent() == 2); // one datagram each, both events in it
    aegis2d::net::ReliableChannel first_client;
    aegis2d::net::ReliableChannel second_client;
    CHECK(events_of(first_client, first_udp.datagrams.at(1)) == spawns);
    CHECK(events_of(second_client, second_udp.datagrams.at(1)) == spawns);

    // To the player who joins only.
    const std::vector<std::vector<std::byte>> world{
        rtype::write_event(rtype::SpawnEvent{.tick = 2, .id = 7, .kind = 4})};
    server.send_events(world, second);
    REQUIRE(pump(context, server, [&] { return second_udp.received.size() == 3; }));
    CHECK(events_of(second_client, second_udp.datagrams.at(2)) == world);
    context.run_for(std::chrono::milliseconds{50});
    CHECK(first_udp.received.size() == 2);

    // Not acknowledged: resent after the delay, as the same events (RFC section 5.3).
    const auto later = std::chrono::milliseconds{150};
    REQUIRE(pump(context, server, [&] { return first_udp.received.size() == 3; }, later));
    CHECK(first_udp.received.at(2).type == PacketType::Event);
    CHECK(first_udp.received.at(2).sequence != first_udp.received.at(1).sequence);
    aegis2d::net::ReliableChannel fresh_client;
    CHECK(events_of(fresh_client, first_udp.datagrams.at(2)) == spawns);

    // Acknowledged: never sent again.
    first_udp.send(server, acknowledging(first, 1, first_udp.received.at(2).sequence));
    REQUIRE(pump(context, server, [&] { return server.game_datagrams() >= 1; }));
    const std::size_t received = first_udp.received.size();
    for (int round = 0; round < 5; ++round) {
        context.run_for(std::chrono::milliseconds{10});
        server.update(ServerNetwork::Clock::now() + std::chrono::seconds{1});
    }
    context.run_for(std::chrono::milliseconds{50});
    CHECK(first_udp.received.size() == received);
}

TEST_CASE("a player whose TCP connection closes is gone from the game within a second",
          "[server][disconnect]") {
    NetworkContext context;
    rtype::GameInstance instance{std::nullopt, aegis2d::ecs::WaveSchedule{}};
    rtype::GameServer game_server{context, instance, 0, 0};
    ServerNetwork& server = game_server.network();

    Player staying_player{context, server};
    const std::uint32_t staying = log_in(context, server, staying_player, "R9");
    Player leaving_player{context, server};
    const std::uint32_t leaving = log_in(context, server, leaving_player, "R10");
    Socket staying_udp{context};
    Socket leaving_udp{context};
    staying_udp.send(server, datagram(PacketType::Connect, staying));
    leaving_udp.send(server, datagram(PacketType::Connect, leaving));

    // What the staying player sees, read from its Event datagrams as a client does.
    aegis2d::net::ReliableChannel client;
    std::vector<rtype::GameEvent> seen;
    std::size_t read = 0;
    const auto watch = [&] {
        for (; read < staying_udp.datagrams.size(); ++read) {
            if (staying_udp.received.at(read).type != PacketType::Event) {
                continue;
            }
            for (const std::vector<std::byte>& bytes :
                 events_of(client, staying_udp.datagrams.at(read))) {
                if (const std::optional<rtype::GameEvent> event = rtype::read_event(bytes)) {
                    seen.push_back(*event);
                }
            }
        }
    };
    const auto ships_of = [&](std::uint8_t player) {
        std::vector<std::uint32_t> ships;
        for (const rtype::GameEvent& event : seen) {
            const auto* const spawn = std::get_if<rtype::SpawnEvent>(&event);
            if (spawn != nullptr && spawn->owner == player &&
                spawn->kind == static_cast<std::uint8_t>(rtype::EntityKind::PlayerShip)) {
                ships.push_back(spawn->id);
            }
        }
        return ships;
    };
    const auto left_with = [&](std::uint32_t ship) {
        return std::ranges::any_of(seen, [&](const rtype::GameEvent& event) {
            const auto* const destroy = std::get_if<rtype::DestroyEvent>(&event);
            return destroy != nullptr && destroy->id == ship &&
                   destroy->reason == rtype::DestroyReason::PlayerLeft;
        });
    };
    REQUIRE(pump(context, server, [&] {
        watch();
        return !ships_of(0).empty() && !ships_of(1).empty();
    }));
    const std::uint32_t ship = ships_of(1).front();

    // The client process dies: the system closes its connection, and the game removes its
    // ship for the others.
    const auto closed_at = std::chrono::steady_clock::now();
    leaving_player.connection.close();
    REQUIRE(pump(context, server, [&] {
        watch();
        return left_with(ship);
    }));
    CHECK(std::chrono::steady_clock::now() - closed_at < std::chrono::seconds{1});
    CHECK(server.sessions().size() == 1);

    // Its UDP endpoint is unbound with the session: its datagrams are dropped.
    const std::uint64_t dropped = server.dropped_datagrams();
    leaving_udp.send(server, datagram(PacketType::Heartbeat, leaving, 1));
    REQUIRE(pump(context, server, [&] { return server.dropped_datagrams() == dropped + 1; }));

    // The player number is free again: a new client gets it, with a new ship.
    Player back_player{context, server};
    const std::uint32_t back = log_in(context, server, back_player, "R10");
    Socket back_udp{context};
    back_udp.send(server, datagram(PacketType::Connect, back));
    REQUIRE(pump(context, server, [&] {
        watch();
        return ships_of(1).size() == 2;
    }));
    CHECK(ships_of(1).back() != ship);
}

TEST_CASE("a bound player silent for 5 seconds loses its session", "[server][timeout]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<unsigned> left;
    server.on_players([](const rtype::Session& /*session*/) {},
                      [&](const rtype::Session& session, rtype::LeaveReason /*reason*/) {
                          left.push_back(session.player);
                      });
    Player talking_player{context, server};
    const std::uint32_t talking = log_in(context, server, talking_player, "R9");
    Player silent_player{context, server};
    const std::uint32_t silent = log_in(context, server, silent_player, "R10");
    Socket talking_udp{context};
    Socket silent_udp{context};
    talking_udp.send(server, datagram(PacketType::Connect, talking, 0));
    silent_udp.send(server, datagram(PacketType::Connect, silent, 0));
    REQUIRE(pump(context, server, [&] {
        return talking_udp.received.size() == 1 && silent_udp.received.size() == 1;
    }));
    const auto bound_at = ServerNetwork::Clock::now();

    // Just under the timeout since the binding: nobody is lost.
    server.update(bound_at + ServerNetwork::udp_timeout - std::chrono::milliseconds{100});
    CHECK(server.timeouts() == 0);

    // Half a second later, one player sends a Heartbeat, the other only a malformed
    // datagram, which does not keep a session alive (RFC section 5.5).
    context.run_for(std::chrono::milliseconds{500});
    talking_udp.send(server, datagram(PacketType::Heartbeat, talking, 1));
    silent_udp.send(server, datagram(PacketType::Heartbeat, silent, 1, {std::byte{0}}));
    REQUIRE(pump(context, server,
                 [&] { return server.game_datagrams() == 1 && server.dropped_datagrams() == 1; }));

    // The timeout is past for the silent player only.
    const auto later = ServerNetwork::udp_timeout - std::chrono::milliseconds{250};
    REQUIRE(pump(context, server, [&] { return silent_player.closed; }, later));
    CHECK(server.timeouts() == 1);
    CHECK(left == std::vector<unsigned>{1});
    CHECK(server.sessions().size() == 1);
    CHECK(server.sessions().find(talking) != nullptr);
    CHECK(!talking_player.closed);
    // Told why before the connection closed (RFC section 3.2).
    REQUIRE(!silent_player.received.empty());
    CHECK(silent_player.received.back() == rtype::write_logout(LogoutReason::TimedOut));

    // Its endpoint is unbound: what it sends now is dropped.
    const std::uint64_t dropped = server.dropped_datagrams();
    silent_udp.send(server, datagram(PacketType::Heartbeat, silent, 2));
    REQUIRE(pump(context, server, [&] { return server.dropped_datagrams() == dropped + 1; }));
}

TEST_CASE("a player whose UDP traffic stops is gone from the game", "[server][timeout]") {
    NetworkContext context;
    rtype::GameInstance instance{std::nullopt, aegis2d::ecs::WaveSchedule{}};
    rtype::GameServer game_server{context, instance, 0, 0};
    ServerNetwork& server = game_server.network();

    Player staying_player{context, server};
    const std::uint32_t staying = log_in(context, server, staying_player, "R9");
    Player blocked_player{context, server};
    const std::uint32_t blocked = log_in(context, server, blocked_player, "R10");
    Socket staying_udp{context};
    Socket blocked_udp{context};
    staying_udp.send(server, datagram(PacketType::Connect, staying));
    blocked_udp.send(server, datagram(PacketType::Connect, blocked));

    // The staying player keeps talking, and reads its events as a client does.
    aegis2d::net::ReliableChannel client;
    std::vector<rtype::GameEvent> seen;
    std::size_t read = 0;
    std::uint16_t sequence = 1;
    const auto talk_and_watch = [&] {
        staying_udp.send(server, datagram(PacketType::Heartbeat, staying, sequence++));
        for (; read < staying_udp.datagrams.size(); ++read) {
            if (staying_udp.received.at(read).type != PacketType::Event) {
                continue;
            }
            for (const std::vector<std::byte>& bytes :
                 events_of(client, staying_udp.datagrams.at(read))) {
                if (const std::optional<rtype::GameEvent> event = rtype::read_event(bytes)) {
                    seen.push_back(*event);
                }
            }
        }
    };
    const auto ship_of = [&](std::uint8_t player) -> std::optional<std::uint32_t> {
        for (const rtype::GameEvent& event : seen) {
            const auto* const spawn = std::get_if<rtype::SpawnEvent>(&event);
            if (spawn != nullptr && spawn->owner == player &&
                spawn->kind == static_cast<std::uint8_t>(rtype::EntityKind::PlayerShip)) {
                return spawn->id;
            }
        }
        return std::nullopt;
    };
    REQUIRE(pump(context, server, [&] {
        talk_and_watch();
        return ship_of(0) && ship_of(1);
    }));
    const std::uint32_t ship = ship_of(1).value_or(0);

    // The blocked player's datagrams stop; past the timeout, its ship leaves the game.
    context.run_for(std::chrono::milliseconds{500});
    const auto later = ServerNetwork::udp_timeout - std::chrono::milliseconds{250};
    REQUIRE(pump(
        context, server,
        [&] {
            talk_and_watch();
            return std::ranges::any_of(seen, [&](const rtype::GameEvent& event) {
                const auto* const destroy = std::get_if<rtype::DestroyEvent>(&event);
                return destroy != nullptr && destroy->id == ship &&
                       destroy->reason == rtype::DestroyReason::PlayerLeft;
            });
        },
        later));
    CHECK(server.timeouts() == 1);
    CHECK(server.sessions().size() == 1);
    REQUIRE(pump(context, server, [&] { return blocked_player.closed; }));
}

TEST_CASE("the game is told why each player left", "[server][leave]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<std::pair<unsigned, rtype::LeaveReason>> left;
    server.on_players([](const rtype::Session& /*session*/) {},
                      [&](const rtype::Session& session, rtype::LeaveReason reason) {
                          left.emplace_back(session.player, reason);
                      });
    std::vector<std::unique_ptr<Player>> players;
    std::vector<std::unique_ptr<Socket>> sockets;
    std::vector<std::uint32_t> tokens;
    const auto bind_player = [&](const std::string& name) {
        players.push_back(std::make_unique<Player>(context, server));
        tokens.push_back(log_in(context, server, *players.back(), name));
        sockets.push_back(std::make_unique<Socket>(context));
        sockets.back()->send(server, datagram(PacketType::Connect, tokens.back()));
        REQUIRE(pump(context, server, [&] { return sockets.back()->received.size() == 1; }));
    };
    for (const std::string name : {"p0", "p1", "p2", "p3"}) {
        bind_player(name);
    }

    // Player 0 logs out, 1 breaks the TCP rules, 2 sends Disconnect, 3's connection closes.
    players.at(0)->send(rtype::write_logout(LogoutReason::Left));
    players.at(1)->send(rtype::write_login_request(request("again")));
    sockets.at(2)->send(server, datagram(PacketType::Disconnect, tokens.at(2), 1));
    players.at(3)->connection.close();
    REQUIRE(pump(context, server, [&] { return left.size() == 4; }));
    std::ranges::sort(left);
    CHECK(left ==
          std::vector<std::pair<unsigned, rtype::LeaveReason>>{{0, rtype::LeaveReason::Left},
                                                               {1, rtype::LeaveReason::Kicked},
                                                               {2, rtype::LeaveReason::Left},
                                                               {3, rtype::LeaveReason::Left}});

    // A silent player times out.
    left.clear();
    bind_player("p4");
    REQUIRE(pump(context, server, [&] { return left.size() == 1; }, ServerNetwork::udp_timeout));
    CHECK(left.front().second == rtype::LeaveReason::TimedOut);
}

namespace {

// The events a client reads from the Event datagrams of its socket.
struct EventLog {
    aegis2d::net::ReliableChannel channel;
    std::vector<rtype::GameEvent> events;
    std::size_t read = 0;

    void update(const Socket& socket) {
        for (; read < socket.datagrams.size(); ++read) {
            if (socket.received.at(read).type != PacketType::Event) {
                continue;
            }
            for (const std::vector<std::byte>& bytes :
                 events_of(channel, socket.datagrams.at(read))) {
                if (const std::optional<rtype::GameEvent> event = rtype::read_event(bytes)) {
                    events.push_back(*event);
                }
            }
        }
    }

    // Index of the first event matching the predicate, or nothing.
    template <typename Event, typename Predicate>
    [[nodiscard]] std::optional<std::size_t> find(const Predicate& predicate) const {
        for (std::size_t index = 0; index < events.size(); ++index) {
            const auto* const event = std::get_if<Event>(&events.at(index));
            if (event != nullptr && predicate(*event)) {
                return index;
            }
        }
        return std::nullopt;
    }
};

} // namespace

TEST_CASE("players learn who joins, and who leaves before its ship is destroyed",
          "[server][leave]") {
    NetworkContext context;
    rtype::GameInstance instance{std::nullopt, aegis2d::ecs::WaveSchedule{}};
    rtype::GameServer game_server{context, instance, 0, 0};
    ServerNetwork& server = game_server.network();

    Player first_player{context, server};
    const std::uint32_t first = log_in(context, server, first_player, "R9");
    Socket first_udp{context};
    first_udp.send(server, datagram(PacketType::Connect, first));
    EventLog first_log;
    const auto ship_of = [](const EventLog& log, std::uint8_t player) {
        return log.find<rtype::SpawnEvent>([&](const rtype::SpawnEvent& spawn) {
            return spawn.owner == player &&
                   spawn.kind == static_cast<std::uint8_t>(rtype::EntityKind::PlayerShip);
        });
    };
    REQUIRE(pump(context, server, [&] {
        first_log.update(first_udp);
        return ship_of(first_log, 0).has_value();
    }));

    // The second player: the first one learns its name, it learns the first one's, before
    // the world, and never gets its own.
    Player second_player{context, server};
    const std::uint32_t second = log_in(context, server, second_player, "R10");
    Socket second_udp{context};
    second_udp.send(server, datagram(PacketType::Connect, second));
    EventLog second_log;
    REQUIRE(pump(context, server, [&] {
        first_log.update(first_udp);
        second_log.update(second_udp);
        return ship_of(first_log, 1) && ship_of(second_log, 0) && ship_of(second_log, 1);
    }));
    const auto joined = [](std::uint8_t player, const std::string& name) {
        return [=](const rtype::PlayerJoinedEvent& event) {
            return event.player == player && event.name == name;
        };
    };
    CHECK(first_log.find<rtype::PlayerJoinedEvent>(joined(1, "R10")).has_value());
    CHECK(!first_log.find<rtype::PlayerJoinedEvent>(joined(0, "R9")).has_value());
    const std::optional<std::size_t> knows_first =
        second_log.find<rtype::PlayerJoinedEvent>(joined(0, "R9"));
    REQUIRE(knows_first.has_value());
    CHECK(knows_first < ship_of(second_log, 0));
    CHECK(!second_log.find<rtype::PlayerJoinedEvent>(joined(1, "R10")).has_value());

    // The second player breaks the TCP rules: the first one learns it was kicked, then sees
    // its ship go.
    const std::uint32_t ship =
        std::get<rtype::SpawnEvent>(first_log.events.at(ship_of(first_log, 1).value_or(0))).id;
    second_player.send(rtype::write_login_request(request("again")));
    const auto destroyed = [&](const rtype::DestroyEvent& event) {
        return event.id == ship && event.reason == rtype::DestroyReason::PlayerLeft;
    };
    REQUIRE(pump(context, server, [&] {
        first_log.update(first_udp);
        return first_log.find<rtype::DestroyEvent>(destroyed).has_value();
    }));
    const std::optional<std::size_t> left_at =
        first_log.find<rtype::PlayerLeftEvent>([](const rtype::PlayerLeftEvent& event) {
            return event.player == 1 && event.reason == rtype::LeaveReason::Kicked;
        });
    REQUIRE(left_at.has_value());
    CHECK(left_at < first_log.find<rtype::DestroyEvent>(destroyed));
}

TEST_CASE("an exception while handling a client kicks that client only", "[server][errors]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<std::pair<unsigned, rtype::LeaveReason>> left;
    std::vector<unsigned> inputs;
    bool throw_on_leave = false;
    server.on_players([](const rtype::Session& /*session*/) {},
                      [&](const rtype::Session& session, rtype::LeaveReason reason) {
                          left.emplace_back(session.player, reason);
                          if (throw_on_leave) {
                              throw std::runtime_error{"left handler failed"};
                          }
                      });
    // A bug in the game's handling of player 1's inputs, standing for any exception.
    server.on_input([&](const rtype::Session& session, aegis2d::ecs::PlayerInput /*input*/,
                        std::uint16_t /*sequence*/) {
        if (session.player == 1) {
            throw std::runtime_error{"input handler failed"};
        }
        inputs.push_back(session.player);
    });
    Player good_player{context, server};
    const std::uint32_t good = log_in(context, server, good_player, "R9");
    Player bad_player{context, server};
    const std::uint32_t bad = log_in(context, server, bad_player, "R10");
    Socket good_udp{context};
    Socket bad_udp{context};
    good_udp.send(server, datagram(PacketType::Connect, good));
    bad_udp.send(server, datagram(PacketType::Connect, bad));
    REQUIRE(pump(context, server,
                 [&] { return good_udp.received.size() == 1 && bad_udp.received.size() == 1; }));

    // Over UDP: the datagram that threw kicks its sender (RFC section 6.6), not the others.
    bad_udp.send(server, datagram(PacketType::Input, bad, 1, input_payload(0, 0x01)));
    REQUIRE(pump(context, server, [&] { return bad_player.closed; }));
    CHECK(server.errors() == 1);
    REQUIRE(!bad_player.received.empty());
    CHECK(bad_player.received.back() == rtype::write_logout(LogoutReason::ProtocolError));
    CHECK(left ==
          std::vector<std::pair<unsigned, rtype::LeaveReason>>{{1, rtype::LeaveReason::Kicked}});
    good_udp.send(server, datagram(PacketType::Input, good, 1, input_payload(0, 0x01)));
    REQUIRE(pump(context, server, [&] { return inputs.size() == 1; }));
    CHECK(inputs.front() == 0);

    // Over TCP: the session is gone even though the handler threw, and the server goes on.
    throw_on_leave = true;
    good_player.send(rtype::write_logout(LogoutReason::Left));
    REQUIRE(pump(context, server, [&] { return good_player.closed; }));
    CHECK(server.errors() == 2);
    CHECK(server.sessions().size() == 0);
    throw_on_leave = false;
    Player next_player{context, server};
    static_cast<void>(log_in(context, server, next_player, "R11"));
    CHECK(server.sessions().size() == 1);
}

TEST_CASE("a flood of forged packets from players in the game crashes nothing",
          "[server][errors]") {
    NetworkContext context;
    rtype::GameInstance instance{std::nullopt, aegis2d::ecs::WaveSchedule{}};
    rtype::GameServer game_server{context, instance, 0, 0};
    ServerNetwork& server = game_server.network();

    // An honest player, which must go on playing.
    Player honest_player{context, server};
    const std::uint32_t honest = log_in(context, server, honest_player, "R9");
    Socket honest_udp{context};
    honest_udp.send(server, datagram(PacketType::Connect, honest));
    REQUIRE(pump(context, server, [&] { return honest_udp.received.size() >= 2; }));

    // Three attackers, logged in and bound, so their forgeries pass the source check.
    std::vector<std::unique_ptr<Player>> attackers;
    std::vector<std::unique_ptr<Socket>> sockets;
    std::vector<std::uint32_t> tokens;
    for (const std::string name : {"a1", "a2", "a3"}) {
        attackers.push_back(std::make_unique<Player>(context, server));
        tokens.push_back(log_in(context, server, *attackers.back(), name));
        sockets.push_back(std::make_unique<Socket>(context));
        sockets.back()->send(server, datagram(PacketType::Connect, tokens.back()));
    }
    REQUIRE(pump(context, server, [&] { return server.sessions().size() == 4; }));

    aegis2d::core::Random random{119};
    const auto random_bytes = [&](std::size_t size) {
        std::vector<std::byte> bytes(size);
        for (std::byte& byte : bytes) {
            byte = static_cast<std::byte>(random.below(256));
        }
        return bytes;
    };
    const std::vector<PacketType> types{
        PacketType::Connect, PacketType::Accept,    PacketType::Input,     PacketType::Snapshot,
        PacketType::Event,   PacketType::Heartbeat, PacketType::Disconnect};
    for (int round = 0; round < 10000; ++round) {
        const std::size_t attacker = random.below(3);
        std::vector<std::byte> forged;
        switch (random.below(3)) {
        case 0:                                        // noise
            forged = random_bytes(random.below(1201)); // the most a socket sends
            break;
        case 1: // a valid header of the attacker, any payload
            forged = datagram(types.at(random.below(types.size())), tokens.at(attacker),
                              static_cast<std::uint16_t>(round), random_bytes(random.below(64)));
            break;
        default: { // a valid datagram with a few bytes changed
            forged = datagram(types.at(random.below(types.size())), tokens.at(attacker),
                              static_cast<std::uint16_t>(round), random_bytes(random.below(8)));
            for (std::uint32_t flips = 1 + random.below(3); flips > 0; --flips) {
                forged.at(random.below(static_cast<std::uint32_t>(forged.size()))) =
                    static_cast<std::byte>(random.below(256));
            }
            break;
        }
        }
        sockets.at(attacker)->send(server, forged);
        if (round % 100 == 0) {
            // And garbage over TCP, which closes the attacker's connection.
            attackers.at(attacker)->send(random_bytes(1 + random.below(64)));
            context.run_for(std::chrono::milliseconds{1});
            server.update(ServerNetwork::Clock::now());
        }
    }

    // The honest player still gets its snapshots and its session; nothing threw.
    const std::size_t before = honest_udp.received.size();
    REQUIRE(pump(context, server, [&] { return honest_udp.received.size() > before + 5; }));
    CHECK(server.sessions().find(honest) != nullptr);
    CHECK(!honest_player.closed);
    CHECK(server.errors() == 0);
}

TEST_CASE("a shutdown logs every player out, then the server goes idle", "[server][shutdown]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<unsigned> left;
    server.on_players([](const rtype::Session& /*session*/) {},
                      [&](const rtype::Session& session, rtype::LeaveReason /*reason*/) {
                          left.push_back(session.player);
                      });
    Player bound{context, server};
    const std::uint32_t token = log_in(context, server, bound, "R9");
    Socket udp{context};
    udp.send(server, datagram(PacketType::Connect, token, 0));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 1; }));
    Player logged{context, server};
    static_cast<void>(log_in(context, server, logged, "R10"));
    Player anonymous{context, server};
    const std::uint16_t port = server.tcp_endpoint().port();
    CHECK(!server.idle());

    server.shutdown();
    REQUIRE(
        pump(context, server, [&] { return bound.closed && logged.closed && anonymous.closed; }));
    REQUIRE(pump(context, server, [&] { return server.idle(); }));

    // The players got Logout(Shutdown) before the close; the others nothing.
    for (const Player* const player : {&bound, &logged}) {
        REQUIRE(player->received.size() == 2);
        CHECK(player->received.back() == rtype::write_logout(LogoutReason::Shutdown));
    }
    CHECK(anonymous.received.empty());
    CHECK(server.sessions().size() == 0);
    CHECK(left == std::vector<unsigned>{0});

    // No new connection is accepted. Windows retries a refused connection for about two
    // seconds before reporting it, longer than pump() waits.
    TcpConnection late{context};
    std::optional<std::error_code> connected;
    late.connect(Endpoint::loopback(port),
                 [&](const std::error_code& error) { connected = error; });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!connected.has_value() && std::chrono::steady_clock::now() < deadline) {
        context.run_for(std::chrono::milliseconds{10});
    }
    REQUIRE(connected.has_value());
    CHECK(connected != std::error_code{});
}

namespace {

// A loopback address other than 127.0.0.1, for a flooder that is not the players.
const Endpoint flooder_address = Endpoint::v4({127, 0, 0, 2}, 0);

} // namespace

TEST_CASE("a flooding address is cut off, and the players play on", "[server][rate]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    server.set_limits(rtype::RateLimits{.datagrams_per_second = 50, .connections_per_address = 8});
    Player player{context, server};
    const std::uint32_t token = log_in(context, server, player, "R9");
    Socket udp{context};
    udp.send(server, datagram(PacketType::Connect, token, 0));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 1; }));

    UdpSocket flooder{context, flooder_address};
    const std::vector<std::byte> junk(32, std::byte{0xAB});
    for (int sent = 0; sent < 200; ++sent) {
        flooder.send_to(Endpoint::loopback(server.udp_endpoint().port()), junk);
    }
    udp.send(server, datagram(PacketType::Heartbeat, token, 1));
    REQUIRE(pump(context, server, [&] { return server.game_datagrams() == 1; }));
    REQUIRE(pump(context, server, [&] { return server.datagrams() >= 202; }));

    // At most 50 of the 200 were read; the player's heartbeat went through.
    CHECK(server.rate_limited() >= 150);
    CHECK(server.dropped_datagrams() == 200);
    CHECK(!player.closed);
    CHECK(server.sessions().size() == 1);
}

TEST_CASE("an address cannot open more connections than its share", "[server][rate]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    server.set_limits(rtype::RateLimits{.datagrams_per_second = 300, .connections_per_address = 2});
    Player first{context, server};
    Player second{context, server};
    Player third{context, server};
    REQUIRE(pump(context, server, [&] { return third.closed; }));
    CHECK(server.refused_connections() == 1);
    CHECK(!first.closed);
    CHECK(!second.closed);

    // Once one closes, a new connection gets its place.
    first.connection.close();
    REQUIRE(pump(context, server, [&] { return server.open_connections() == 1; }));
    Player fourth{context, server};
    static_cast<void>(log_in(context, server, fourth, "R9"));
    CHECK(server.refused_connections() == 1);
}

TEST_CASE("a flood does not slow the tick rate", "[server][rate]") {
    NetworkContext context;
    rtype::GameInstance instance{std::nullopt, aegis2d::ecs::WaveSchedule{}};
    rtype::GameServer game_server{context, instance, 0, 0};
    ServerNetwork& server = game_server.network();
    const std::uint16_t port = server.udp_endpoint().port();

    // A second context and thread flood the server as fast as they can for a second, while
    // the server's context handles them and the simulation ticks. A jthread, so that it stops
    // and is joined even when an assertion throws.
    NetworkContext flood_context;
    UdpSocket flooder{flood_context, flooder_address};
    std::jthread flood{[&](const std::stop_token& stop) {
        const std::vector<std::byte> junk(1200, std::byte{0xAB});
        while (!stop.stop_requested()) {
            for (int burst = 0; burst < 100; ++burst) {
                flooder.send_to(Endpoint::loopback(port), junk);
            }
            flood_context.poll();
        }
    }};
    const std::uint64_t first = game_server.simulation().ticks();
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds{1}) {
        context.run_for(std::chrono::milliseconds{10});
        server.update(ServerNetwork::Clock::now());
    }
    const std::uint64_t ticks = game_server.simulation().ticks() - first;
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    flood.request_stop();
    flood.join();

    const double rate = static_cast<double>(ticks) / elapsed.count();
    CAPTURE(ticks, rate, server.datagrams(), server.rate_limited());
    CHECK(server.rate_limited() > 0);
    CHECK(rate > 50.0);
}
