#include <aegis2d/core/log.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <rtype/server_network.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace rtype {

namespace net = aegis2d::net;

ServerNetwork::ServerNetwork(net::NetworkContext& context, std::uint16_t tcp_port,
                             std::uint16_t udp_port)
    : listener_{context, tcp_port}, socket_{context, udp_port} {
    // Until the login exists, a connection is accepted and dropped: its destructor closes it.
    listener_.start_accepting([this](net::TcpConnection connection) {
        ++connections_;
        aegis2d::core::log_info("TCP: connection from {} closed: logins are not handled yet",
                                connection.remote_endpoint().to_string());
    });
    socket_.start_receiving([](const net::Endpoint& from, std::span<const std::byte> datagram) {
        aegis2d::core::log_debug("UDP: dropped a {}-byte datagram from {}: no session yet",
                                 datagram.size(), from.to_string());
    });
}

net::Endpoint ServerNetwork::tcp_endpoint() const {
    return listener_.local_endpoint();
}

net::Endpoint ServerNetwork::udp_endpoint() const {
    return socket_.local_endpoint();
}

std::uint64_t ServerNetwork::connections() const noexcept {
    return connections_;
}

std::uint64_t ServerNetwork::datagrams() const noexcept {
    return socket_.stats().received;
}

} // namespace rtype
