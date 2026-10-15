/// @file
/// @brief Sockets of the R-Type server: the TCP port of the logins and the UDP port of the game.
#pragma once
#include <aegis2d/net/binary.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/reliability.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <rtype/sessions.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>

namespace rtype {

/// @brief The two ports the server listens on, and the TCP side of the sessions.
/// @details Binds both ports in its constructor, so a port already in use stops the server
/// at start. Each TCP connection must log in (RFC section 3.3): a valid `LoginRequest`
/// opens a session and gets `LoginAccepted`; a refused one gets `LoginRejected` and is
/// closed once it is sent; a connection that sends anything else first, or nothing within
/// login_timeout, is closed without a reply (section 6.5). Once logged in, a `Logout` or
/// the closing of the connection ends the session; any other message is a protocol error
/// that ends it too.
///
/// Each datagram is checked as RFC section 6.3 says (header, direction, source). A
/// `Connect` with an empty payload and the token of a session binds its source endpoint
/// to the session and is answered with `Accept`, again for each `Connect` from that
/// endpoint (section 3.4). Every other datagram must come from a bound endpoint with the
/// token of its session. Datagrams that fail a check are dropped without an answer and
/// counted; the valid ones are counted until the game loop reads them (#112).
///
/// Connections and datagrams are handled on the thread running the context; update() runs
/// the timeouts and removes closed connections. Create, use and destroy it on that thread,
/// or while the context is not running.
class ServerNetwork {
public:
    /// @brief Clock of the login timeout.
    using Clock = std::chrono::steady_clock;

    /// @brief Time a new connection has to send its `LoginRequest`.
    static constexpr std::chrono::seconds login_timeout{5};

    /// @brief Listens on both ports, on every IPv4 interface.
    /// @param context Context that runs the sockets; it must outlive this object.
    /// @param tcp_port TCP port of the logins, or 0 for any free port.
    /// @param udp_port UDP port of the game, or 0 for any free port.
    /// @param sessions Sessions to fill, empty; tests give one with a seed.
    /// @throws aegis2d::net::SocketError If a port is already in use or cannot be bound.
    ServerNetwork(aegis2d::net::NetworkContext& context, std::uint16_t tcp_port,
                  std::uint16_t udp_port, SessionTable sessions = SessionTable{});

    ServerNetwork(const ServerNetwork&) = delete;
    ServerNetwork& operator=(const ServerNetwork&) = delete;
    ServerNetwork(ServerNetwork&&) = delete;
    ServerNetwork& operator=(ServerNetwork&&) = delete;
    ~ServerNetwork() = default;

    /// @brief Closes the connections that did not log in within login_timeout, and those
    /// whose rejection is sent, then forgets the closed ones.
    /// @details Call it regularly, a few times per second at least.
    /// @param now Current time.
    void update(Clock::time_point now);

    /// @brief Local TCP address and port, with the port the system picked if 0 was asked.
    /// @return The bound endpoint.
    [[nodiscard]] aegis2d::net::Endpoint tcp_endpoint() const;

    /// @brief Local UDP address and port, with the port the system picked if 0 was asked.
    /// @return The bound endpoint.
    [[nodiscard]] aegis2d::net::Endpoint udp_endpoint() const;

    /// @brief Sessions open.
    /// @return The table of the logged-in players.
    [[nodiscard]] const SessionTable& sessions() const noexcept;

    /// @brief Number of TCP connections accepted.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t connections() const noexcept;

    /// @brief Number of TCP connections not removed yet by update(), logged in or not.
    /// @return The count.
    [[nodiscard]] std::size_t open_connections() const noexcept;

    /// @brief Number of UDP datagrams received, oversized ones excluded.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t datagrams() const noexcept;

    /// @brief Number of UDP datagrams dropped for their header, direction or source.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t dropped_datagrams() const noexcept;

    /// @brief Number of datagrams from bound endpoints, other than `Connect`, for the game.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t game_datagrams() const noexcept;

private:
    struct Client {
        aegis2d::net::TcpConnection connection;
        Clock::time_point accepted;
        std::optional<std::uint32_t> token;
        std::uint64_t sent = 0;
        bool close_when_sent = false;
        bool closed = false;
    };

    void on_accept(aegis2d::net::TcpConnection connection);
    void on_message(std::uint64_t id, std::span<const std::byte> message);
    void on_login(Client& client, std::span<const std::byte> message);
    void on_close(std::uint64_t id, aegis2d::net::TcpCloseReason reason);
    void on_datagram(const aegis2d::net::Endpoint& from, std::span<const std::byte> datagram);
    void on_connect(const aegis2d::net::Endpoint& from, std::span<const std::byte> datagram,
                    const aegis2d::net::PacketHeader& header,
                    const aegis2d::net::BinaryReader& payload);
    void drop(const aegis2d::net::Endpoint& from, std::span<const std::byte> datagram,
              std::string_view reason);
    void end_session(std::uint32_t token);
    void send(Client& client, std::span<const std::byte> message);
    void close(Client& client, std::string_view reason);

    aegis2d::net::TcpListener listener_;
    aegis2d::net::UdpSocket socket_;
    SessionTable sessions_;
    std::unordered_map<std::uint64_t, Client> clients_;
    // Sequences and acknowledgements of each bound session's datagrams, by token.
    std::unordered_map<std::uint32_t, aegis2d::net::ReliableChannel> channels_;
    std::uint64_t connections_ = 0;
    std::uint64_t dropped_ = 0;
    std::uint64_t game_datagrams_ = 0;
};

} // namespace rtype
