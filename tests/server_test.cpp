#include <aegis2d/net/binary.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/reliability.hpp>
#include <aegis2d/net/socket_error.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/events.hpp>
#include <rtype/login.hpp>
#include <rtype/server_network.hpp>
#include <rtype/server_options.hpp>
#include <rtype/sessions.hpp>
#include <rtype/snapshot.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
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

// Runs the context until the condition holds, for at most two seconds.
template <typename Condition>
bool run_until(NetworkContext& context, Condition condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        context.run_for(std::chrono::milliseconds{10});
    }
    return true;
}

// Runs the context and the server's update() until the condition holds, for at most two
// seconds, the server's clock being shifted by offset.
template <typename Condition>
bool pump(NetworkContext& context, ServerNetwork& server, Condition condition,
          ServerNetwork::Clock::duration offset = {}) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
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

    Player(NetworkContext& context, ServerNetwork& server) : connection{context} {
        std::optional<std::error_code> connected;
        connection.connect(Endpoint::loopback(server.tcp_endpoint().port()),
                           [&](const std::error_code& error) { connected = error; });
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
        datagram(PacketType::Input, token, 0, {std::byte{0x01}}), // endpoint not bound
        datagram(PacketType::Connect, token),                     // session bound elsewhere
        datagram(PacketType::Connect, token + 1),                 // unknown session
        datagram(PacketType::Connect, 0),                         // 0 is never a token
        datagram(PacketType::Snapshot, token),                    // wrong direction
        {std::byte{0x52}, std::byte{0x54}, std::byte{0x59}},      // truncated header
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
                      [&](const rtype::Session& session) { left.push_back(session.player); });

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
    std::vector<unsigned> left;
    server.on_input([&](const rtype::Session& /*session*/, aegis2d::ecs::PlayerInput input) {
        inputs.push_back(input.held);
    });
    server.on_players([](const rtype::Session& /*session*/) {},
                      [&](const rtype::Session& session) { left.push_back(session.player); });
    Player player{context, server};
    const std::uint32_t token = log_in(context, server, player, "R9");
    Socket udp{context};
    udp.send(server, datagram(PacketType::Connect, token, 0));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 1; }));

    const auto input = [&](std::uint16_t sequence, std::vector<std::byte> payload) {
        udp.send(server, datagram(PacketType::Input, token, sequence, std::move(payload)));
    };
    input(1, {std::byte{0x08}});               // right: applied
    input(2, {std::byte{0x20}});               // bit 5 set: malformed
    input(3, {std::byte{0x08}, std::byte{0}}); // two bytes: malformed
    input(5, {std::byte{0x01}});               // up: applied
    input(4, {std::byte{0x02}});               // older than 5: ignored
    input(5, {std::byte{0x01}});               // duplicate: ignored
    REQUIRE(pump(context, server, [&] { return server.dropped_datagrams() == 2; }));
    REQUIRE(pump(context, server, [&] { return server.game_datagrams() == 4; }));
    CHECK(inputs == std::vector<std::uint8_t>{0x08, 0x01});

    // The next datagram to the player acknowledges 6, 5, 4, 1 and 0, not the malformed 2 and 3.
    udp.send(server, datagram(PacketType::Connect, token, 6));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 2; }));
    CHECK(udp.received.back().ack == 6);
    CHECK(udp.received.back().ack_bits == 0b110011U);
}

TEST_CASE("heartbeats, events and disconnects of a bound player are checked", "[server][input]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    std::vector<unsigned> left;
    server.on_players([](const rtype::Session& /*session*/) {},
                      [&](const rtype::Session& session) { left.push_back(session.player); });
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
    udp.send(server, datagram(PacketType::Input, token, 1, {std::byte{0x08}}));
    REQUIRE(pump(context, server, [&] { return server.game_datagrams() == 1; }));

    rtype::Snapshot snapshot{.tick = 9, .players = {}, .entities = {}};
    snapshot.entities.resize(60);
    server.send_snapshot(rtype::write_snapshot(snapshot));
    REQUIRE(pump(context, server, [&] { return udp.received.size() == 3; }));

    // Two datagrams (51 + 9 entities), after the Accept of sequence 0, acknowledging 1 and 0;
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
    CHECK(udp.sizes.at(1) == PacketHeader::size + 7 + (std::size_t{51} * 22));
    CHECK(udp.sizes.at(2) == PacketHeader::size + 7 + (std::size_t{9} * 22));
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
