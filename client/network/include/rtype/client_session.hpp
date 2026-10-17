/// @file
/// @brief The client's session with the server: TCP login, then UDP binding (RFC section 3).
#pragma once
#include <aegis2d/core/clock.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/binary.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/reliability.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <rtype/client_world.hpp>
#include <rtype/login.hpp>
#include <rtype/tuning.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace rtype {

/// @brief Where the client is in its session.
enum class ClientState : std::uint8_t {
    /// @brief Not started.
    Idle,
    /// @brief Opening the TCP connection.
    Connecting,
    /// @brief `LoginRequest` sent, waiting for the answer.
    LoggingIn,
    /// @brief Logged in, sending `Connect` until `Accept` arrives.
    Binding,
    /// @brief UDP endpoint bound: the game may start.
    InGame,
    /// @brief The session could not start or was lost; error() says why.
    Failed,
    /// @brief The player left with leave().
    Closed,
};

/// @brief Short name of a state, for logs.
/// @param state State to name.
/// @return `"idle"`, `"connecting"`, `"logging in"`... or `"unknown"`.
[[nodiscard]] std::string_view to_string(ClientState state) noexcept;

/// @brief What the client needs to join a server.
struct ClientSettings {
    /// @brief Numeric IPv4 or IPv6 address of the server, or `"localhost"`.
    std::string host = "127.0.0.1";
    /// @brief TCP port of the server.
    std::uint16_t tcp_port = 4242;
    /// @brief Player name, 1 to 16 bytes of UTF-8 without control characters.
    std::string name = "player";
};

/// @brief The client side of a session (RFC section 3).
/// @details start() opens the TCP connection and sends `LoginRequest`. On `LoginAccepted`,
/// the client opens a UDP socket and sends `Connect` to the server's `udp_port` every
/// connect_interval until `Accept` arrives: the session is then InGame. It fails, with a
/// reason in error(), when the server cannot be reached, refuses the login, sends anything
/// unexpected, closes the connection, or does not answer: no `LoginAccepted` within
/// login_timeout, or no `Accept` within bind_timeout of `LoginAccepted` (then `Logout` is
/// sent, as the UDP port is likely blocked).
///
/// From the server's UDP traffic, only datagrams from its `udp_port` with the session's
/// token are read (section 3.4); before the first `Accept`, only `Accept`.
///
/// Handlers run on the thread running the context; call update() a few times per second
/// for the timers. Create, use and destroy it on that thread, or while the context is not
/// running.
class ClientSession {
public:
    /// @brief Clock of the timers.
    using Clock = std::chrono::steady_clock;

    /// @brief Time between two `Connect` datagrams (RFC section 3.4).
    static constexpr std::chrono::milliseconds connect_interval{250};
    /// @brief Longest wait for `Accept` after `LoginAccepted` (RFC section 3.4).
    static constexpr std::chrono::seconds bind_timeout{5};
    /// @brief Longest wait for the TCP connection and `LoginAccepted`.
    static constexpr std::chrono::seconds login_timeout{5};
    /// @brief Time between two `Input` datagrams: one per server tick (RFC section 4.2).
    static constexpr aegis2d::core::Duration input_interval =
        aegis2d::core::step_from_rate(tuning::tick_rate);

    /// @brief Creates a session, not started.
    /// @param context Context that runs the sockets; it must outlive this object.
    /// @param settings Server and player name.
    ClientSession(aegis2d::net::NetworkContext& context, ClientSettings settings);

    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    ClientSession(ClientSession&&) = delete;
    ClientSession& operator=(ClientSession&&) = delete;
    /// @brief Closes the sockets, without `Logout`: call leave() first to say goodbye.
    ~ClientSession();

    /// @brief Connects to the server and logs in.
    /// @details Fails at once if the host is not a numeric address or `"localhost"`, or the
    /// name breaks the rules of `LoginRequest`.
    void start();

    /// @brief Runs the timers: `Connect` resent, the login and binding timeouts, and, in game,
    /// one `Input` every input_interval.
    /// @details Inputs keep their rate whatever the calls: one is sent when its time has come,
    /// and after a stall the next one is due an interval later, without a burst.
    /// @param now Current time.
    void update(Clock::time_point now);

    /// @brief Sets the actions the player holds, sent in every `Input` until the next call.
    /// @details The client sends what is held, not what changed, so a lost `Input` is
    /// replaced by the next one (RFC section 4.2). Bits 5 to 7 are cleared.
    /// @param input Held actions.
    void set_input(aegis2d::ecs::PlayerInput input) noexcept;

    /// @brief Leaves the server: sends `Logout` and closes once it has left.
    /// @details The state becomes Closed at once; keep running the context a little for the
    /// `Logout` to leave.
    void leave();

    /// @brief Where the session is.
    /// @return The state.
    [[nodiscard]] ClientState state() const noexcept;

    /// @brief Why the session failed.
    /// @return A sentence for the player, empty unless the state is Failed.
    [[nodiscard]] const std::string& error() const noexcept;

    /// @brief What the server gave at login.
    /// @return Token, player number and UDP port, once logged in.
    [[nodiscard]] const std::optional<LoginAccepted>& login() const noexcept;

    /// @brief The server's UDP endpoint, once logged in.
    /// @return Its address and `udp_port`.
    [[nodiscard]] const std::optional<aegis2d::net::Endpoint>& server_udp() const noexcept;

    /// @brief Number of `Connect` datagrams sent.
    /// @return The count since start().
    [[nodiscard]] std::uint64_t connects_sent() const noexcept;

    /// @brief The world as the server describes it, once in game.
    /// @details Updated with each `Snapshot` and `Event` datagram received (RFC sections 4.3
    /// to 4.5): see ClientWorld.
    /// @return The world.
    [[nodiscard]] ClientWorld& world() noexcept;

    /// @copydoc world()
    [[nodiscard]] const ClientWorld& world() const noexcept;

    /// @brief Number of `Input` datagrams sent.
    /// @return The count since the game state was reached.
    [[nodiscard]] std::uint64_t inputs_sent() const noexcept;

private:
    void fail(std::string reason);
    void on_connected();
    void on_message(std::span<const std::byte> message);
    void on_datagram(const aegis2d::net::Endpoint& from, std::span<const std::byte> datagram);
    void send_connect(Clock::time_point now);
    void send_input(Clock::time_point now);
    void on_game_datagram(const aegis2d::net::PacketHeader& header,
                          aegis2d::net::BinaryReader& payload);

    aegis2d::net::NetworkContext& context_;
    ClientSettings settings_;
    ClientState state_ = ClientState::Idle;
    std::string error_;
    std::unique_ptr<aegis2d::net::TcpConnection> connection_;
    std::unique_ptr<aegis2d::net::UdpSocket> socket_;
    aegis2d::net::ReliableChannel channel_;
    std::optional<aegis2d::net::Endpoint> server_tcp_;
    std::optional<aegis2d::net::Endpoint> server_udp_;
    std::optional<LoginAccepted> login_;
    Clock::time_point started_;
    Clock::time_point accepted_;
    Clock::time_point last_connect_;
    std::uint64_t connects_sent_ = 0;
    std::uint8_t held_ = 0;
    Clock::time_point next_input_;
    std::uint64_t inputs_sent_ = 0;
    ClientWorld world_;
    // Messages handed to the connection, to close it once they have left.
    std::uint64_t sent_ = 0;
    bool closing_ = false;
};

} // namespace rtype
