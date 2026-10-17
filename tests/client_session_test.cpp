#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/login.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using aegis2d::net::Endpoint;
using aegis2d::net::NetworkContext;
using aegis2d::net::TcpConnection;
using aegis2d::net::TcpListener;
using aegis2d::net::UdpSocket;
using Catch::Matchers::ContainsSubstring;
using rtype::ClientSession;
using rtype::ClientSettings;
using rtype::ClientState;
using rtype::ServerNetwork;

namespace {

// Runs the context, the server and the clients until the condition holds, for at most ten
// seconds: Windows reports a refused connection after about two, and a loaded CI runner can
// be seconds late.
template <typename Condition>
bool pump(NetworkContext& context, ServerNetwork* server,
          std::initializer_list<ClientSession*> clients, Condition condition,
          ClientSession::Clock::duration offset = {}) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
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

TEST_CASE("a server that stops ends the session with its reason", "[client]") {
    NetworkContext context;
    ServerNetwork server{context, 0, 0};
    ClientSession client{context, settings_for(server, "R9")};
    client.start();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::InGame; }));
    server.shutdown();
    REQUIRE(
        pump(context, &server, {&client}, [&] { return client.state() == ClientState::Failed; }));
    CHECK(client.error() == "the server stopped");
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
