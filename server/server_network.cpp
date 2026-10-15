#include <aegis2d/core/log.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <rtype/login.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;
namespace core = aegis2d::core;

ServerNetwork::ServerNetwork(net::NetworkContext& context, std::uint16_t tcp_port,
                             std::uint16_t udp_port, SessionTable sessions)
    : listener_{context, tcp_port}, socket_{context, udp_port}, sessions_{std::move(sessions)} {
    listener_.start_accepting(
        [this](net::TcpConnection connection) { on_accept(std::move(connection)); });
    socket_.start_receiving([](const net::Endpoint& from, std::span<const std::byte> datagram) {
        core::log_debug("UDP: dropped a {}-byte datagram from {}: endpoints are not bound yet",
                        datagram.size(), from.to_string());
    });
}

void ServerNetwork::on_accept(net::TcpConnection connection) {
    const std::uint64_t id = connections_++;
    Client& client = clients_
                         .emplace(id, Client{.connection = std::move(connection),
                                             .accepted = Clock::now(),
                                             .token = std::nullopt,
                                             .sent = 0,
                                             .close_when_sent = false,
                                             .closed = false})
                         .first->second;
    // The handlers find the client by id: it may be gone when a late handler runs.
    client.connection.start(
        [this, id](std::span<const std::byte> message) { on_message(id, message); },
        [this, id](net::TcpCloseReason reason) { on_close(id, reason); });
}

void ServerNetwork::on_message(std::uint64_t id, std::span<const std::byte> message) {
    const auto found = clients_.find(id);
    if (found == clients_.end() || found->second.closed || found->second.close_when_sent) {
        return;
    }
    Client& client = found->second;
    if (!client.token) {
        on_login(client, message);
        return;
    }
    // Logged in: a Logout is the only message the client may send (RFC section 3.3).
    const Session* const session = sessions_.find(*client.token);
    const unsigned player = session != nullptr ? session->player : 0U;
    sessions_.end(*client.token);
    if (read_logout(message)) {
        core::log_info("TCP: player {} logged out", player);
        close(client, {});
        return;
    }
    const std::optional<TcpKind> kind = kind_of(message);
    core::log_info("TCP: player {} sent {} after the login: session ended", player,
                   kind == TcpKind::Logout ? "a malformed Logout" : "an unexpected message");
    close(client, {});
}

void ServerNetwork::on_login(Client& client, std::span<const std::byte> message) {
    const std::optional<LoginRequest> request = read_login_request(message);
    if (!request) {
        close(client, kind_of(message) == TcpKind::LoginRequest
                          ? "malformed LoginRequest"
                          : "a message other than LoginRequest first");
        return;
    }
    const std::variant<Session, LoginRejection> result = sessions_.login(*request);
    if (const auto* const rejection = std::get_if<LoginRejection>(&result)) {
        core::log_info("TCP: login from {} refused: {}",
                       client.connection.remote_endpoint().to_string(), to_string(*rejection));
        send(client, write_login_rejected(*rejection));
        client.close_when_sent = true;
        return;
    }
    const auto& session = *std::get_if<Session>(&result);
    client.token = session.token;
    // The name passed valid_player_name(): no control character reaches the log.
    core::log_info("TCP: {} logged in as player {} ({})",
                   client.connection.remote_endpoint().to_string(), session.player, session.name);
    send(client, write_login_accepted(LoginAccepted{
                     .session = session.token,
                     .player = session.player,
                     .udp_port = socket_.local_endpoint().port(),
                 }));
}

void ServerNetwork::on_close(std::uint64_t id, net::TcpCloseReason reason) {
    const auto found = clients_.find(id);
    if (found == clients_.end()) {
        return;
    }
    Client& client = found->second;
    client.closed = true;
    if (client.token) {
        const Session* const session = sessions_.find(*client.token);
        if (session != nullptr) {
            core::log_info("TCP: player {} disconnected", session->player);
        }
        sessions_.end(*client.token);
    } else if (reason == net::TcpCloseReason::MessageTooLarge) {
        core::log_info("TCP: closed {}: message too large",
                       client.connection.remote_endpoint().to_string());
    }
}

void ServerNetwork::send(Client& client, std::span<const std::byte> message) {
    client.connection.send(message);
    ++client.sent;
}

void ServerNetwork::close(Client& client, std::string_view reason) {
    if (!reason.empty()) {
        core::log_info("TCP: closed {}: {}", client.connection.remote_endpoint().to_string(),
                       reason);
    }
    client.connection.close();
    client.closed = true;
}

void ServerNetwork::update(Clock::time_point now) {
    for (auto& [id, client] : clients_) {
        if (client.closed) {
            continue;
        }
        if (client.close_when_sent && client.connection.stats().sent >= client.sent) {
            close(client, {});
        } else if (!client.token && !client.close_when_sent &&
                   now - client.accepted >= login_timeout) {
            close(client, "login timeout");
        }
    }
    std::erase_if(clients_, [](const auto& entry) { return entry.second.closed; });
}

net::Endpoint ServerNetwork::tcp_endpoint() const {
    return listener_.local_endpoint();
}

net::Endpoint ServerNetwork::udp_endpoint() const {
    return socket_.local_endpoint();
}

const SessionTable& ServerNetwork::sessions() const noexcept {
    return sessions_;
}

std::uint64_t ServerNetwork::connections() const noexcept {
    return connections_;
}

std::size_t ServerNetwork::open_connections() const noexcept {
    return clients_.size();
}

std::uint64_t ServerNetwork::datagrams() const noexcept {
    return socket_.stats().received;
}

} // namespace rtype
