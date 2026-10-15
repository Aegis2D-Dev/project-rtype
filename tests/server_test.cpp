#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/socket_error.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/login.hpp>
#include <rtype/server_network.hpp>
#include <rtype/server_options.hpp>
#include <rtype/sessions.hpp>

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
