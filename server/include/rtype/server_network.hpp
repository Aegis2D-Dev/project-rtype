/// @file
/// @brief Sockets of the R-Type server: the TCP port of the logins and the UDP port of the game.
#pragma once
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <cstdint>

namespace rtype {

/// @brief The two ports the server listens on (RFC section 1.3).
/// @details Binds both ports in its constructor, so a port already in use stops the server
/// at start. Connections and datagrams are received on the thread running the context.
/// Logins (#109) and the binding of UDP endpoints (#110) are not handled yet: an accepted
/// connection is closed at once, and datagrams are counted and dropped.
///
/// Create, use and destroy it on the thread that runs the context, or while the context is
/// not running.
class ServerNetwork {
public:
    /// @brief Listens on both ports, on every IPv4 interface.
    /// @param context Context that runs the sockets; it must outlive this object.
    /// @param tcp_port TCP port of the logins, or 0 for any free port.
    /// @param udp_port UDP port of the game, or 0 for any free port.
    /// @throws aegis2d::net::SocketError If a port is already in use or cannot be bound.
    ServerNetwork(aegis2d::net::NetworkContext& context, std::uint16_t tcp_port,
                  std::uint16_t udp_port);

    /// @brief Local TCP address and port, with the port the system picked if 0 was asked.
    /// @return The bound endpoint.
    [[nodiscard]] aegis2d::net::Endpoint tcp_endpoint() const;

    /// @brief Local UDP address and port, with the port the system picked if 0 was asked.
    /// @return The bound endpoint.
    [[nodiscard]] aegis2d::net::Endpoint udp_endpoint() const;

    /// @brief Number of TCP connections accepted.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t connections() const noexcept;

    /// @brief Number of UDP datagrams received, oversized ones excluded.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t datagrams() const noexcept;

private:
    aegis2d::net::TcpListener listener_;
    aegis2d::net::UdpSocket socket_;
    std::uint64_t connections_ = 0;
};

} // namespace rtype
