#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/socket_error.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/server_network.hpp>
#include <rtype/server_options.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using aegis2d::net::Endpoint;
using aegis2d::net::NetworkContext;
using aegis2d::net::SocketError;
using aegis2d::net::TcpConnection;
using aegis2d::net::TcpListener;
using aegis2d::net::UdpSocket;
using rtype::ServerCommand;
using rtype::ServerNetwork;
using rtype::ServerOptions;

namespace {

std::expected<ServerOptions, std::string> parse(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> list{args};
    return rtype::parse_server_options(list);
}

std::string error_of(std::initializer_list<std::string_view> args) {
    const std::expected<ServerOptions, std::string> options = parse(args);
    REQUIRE_FALSE(options.has_value());
    return options.error();
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

} // namespace

TEST_CASE("without arguments the server runs on the default ports", "[server][options]") {
    const std::expected<ServerOptions, std::string> options = parse({});
    REQUIRE(options.has_value());
    CHECK(options->command == ServerCommand::Run);
    CHECK(options->tcp_port == rtype::default_tcp_port);
    CHECK(options->udp_port == rtype::default_udp_port);
    CHECK_FALSE(options->waves.has_value());
}

TEST_CASE("ports and waves file are read in any order", "[server][options]") {
    const std::expected<ServerOptions, std::string> options =
        parse({"--udp-port", "5000", "--waves", "stage2.json", "--tcp-port", "65535"});
    REQUIRE(options.has_value());
    CHECK(options->command == ServerCommand::Run);
    CHECK(options->tcp_port == 65535);
    CHECK(options->udp_port == 5000);
    CHECK(options->waves == std::filesystem::path{"stage2.json"});

    CHECK(parse({"--tcp-port", "0"})->tcp_port == 0);
    CHECK(parse({"--tcp-port", "1", "--tcp-port", "2"})->tcp_port == 2);
}

TEST_CASE("help and version stop the reading", "[server][options]") {
    CHECK(parse({"--help"})->command == ServerCommand::Help);
    CHECK(parse({"--version"})->command == ServerCommand::Version);
    CHECK(parse({"--tcp-port", "1", "--version", "--bad"})->command == ServerCommand::Version);
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
