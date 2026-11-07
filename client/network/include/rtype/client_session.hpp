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
#include <aegis2d/net/rtt.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/traffic.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <rtype/chat_log.hpp>
#include <rtype/client_world.hpp>
#include <rtype/input.hpp>
#include <rtype/login.hpp>
#include <rtype/network_stats.hpp>
#include <rtype/rooms.hpp>
#include <rtype/ship_prediction.hpp>
#include <rtype/snapshot_decoder.hpp>
#include <rtype/tuning.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

/// @brief Where the client is in its session.
enum class ClientState : std::uint8_t {
    /// @brief Not started.
    Idle,
    /// @brief Opening the TCP connection.
    Connecting,
    /// @brief `LoginRequest` sent, waiting for the answer.
    LoggingIn,
    /// @brief Logged in with ClientSettings::choose_room: choosing a room (RFC 3.6, 3.7).
    Lobby,
    /// @brief In the room joined from the lobby, until all its players are ready (RFC 3.7).
    InRoom,
    /// @brief Logged in, sending `Connect` until `Accept` arrives.
    Binding,
    /// @brief UDP endpoint bound: the game may start.
    InGame,
    /// @brief The session could not start; error() says why.
    Failed,
    /// @brief The session was running and ended: the server stopped, went silent, closed the
    /// connection or ended the session; error() says why, for the connection-lost screen.
    Lost,
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
    /// @brief Stays in the lobby after the login, where the rooms are listed, created and
    /// joined, and binds once a room is joined; `false` binds at once, in the room the server
    /// gives at login.
    bool choose_room = false;
};

/// @brief The client side of a session (RFC section 3).
/// @details start() opens the TCP connection and sends `LoginRequest`. On `LoginAccepted`,
/// the client opens a UDP socket and sends `Connect` to the server's `udp_port` every
/// connect_interval until `Accept` arrives: the session is then InGame. With
/// ClientSettings::choose_room, it stays in the Lobby state instead, where rooms() is
/// refreshed every room_refresh, and create_room() and join_room() send their requests. A
/// `RoomJoined` puts it InRoom, with the player's number in that room: room_players() follows
/// the room, set_ready() and leave_room() send their requests, and `GameStart`, once every
/// player of the room is ready, starts the binding. It fails, with a
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
    /// @brief Longest silence of the server in game before the session is lost (RFC 5.5).
    static constexpr std::chrono::seconds server_timeout{5};
    /// @brief Time between two `ListRooms` in the lobby.
    static constexpr std::chrono::seconds room_refresh{1};
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

    /// @brief Asks the server for a new room, in the lobby (`CreateRoom`, RFC section 3.6).
    /// @details rooms() lists it once created; room_error() says why it was not.
    /// @param request Name and size of the room.
    void create_room(const CreateRoomRequest& request);

    /// @brief Asks to join a room, in the lobby (`JoinRoom`, RFC section 3.7).
    /// @details On `RoomJoined`, login() takes the player's number in the room and the session
    /// is InRoom; room_error() says why the room was not joined.
    /// @param room Number of the room.
    void join_room(std::uint8_t room);

    /// @brief Says whether the player is ready to start, in a room (`SetReady`, RFC 3.7).
    /// @param ready Ready, or not any more.
    void set_ready(bool ready);

    /// @brief Leaves the room for the lobby, where the rooms are listed again (`LeaveRoom`).
    void leave_room();

    /// @brief Sends a chat message to the players of the room, or of the lobby (`ChatSend`,
    /// RFC section 3.8).
    /// @details Only once logged in, until the session ends; the server's `ChatMessage`
    /// puts it in chat() for every player of the room, this one too, or its `ChatRejected`
    /// the reason it was not sent.
    /// @param text Text typed; it must fit a `string`.
    /// @return `false` if the session is not logged in or has ended: nothing was sent.
    bool send_chat(std::string_view text);

    /// @brief The last chat messages of the room, and why a message of this client was not
    /// sent, each for a few seconds.
    /// @return The log; ChatLog::prune() it before drawing.
    [[nodiscard]] ChatLog& chat() noexcept;

    /// @brief Whether the player said it is ready, in its room.
    /// @return The last value given to set_ready() since the room was joined.
    [[nodiscard]] bool ready() const noexcept;

    /// @brief The players of the room joined, from the server's last `RoomPlayers`.
    /// @return The room and its players, by number; no player before the first one.
    [[nodiscard]] const RoomPlayers& room_players() const noexcept;

    /// @brief The rooms of the server, from its last `RoomList`.
    /// @return Them, by number; none before the first answer.
    [[nodiscard]] const std::vector<RoomRecord>& rooms() const noexcept;

    /// @brief Why the last room could not be created or joined.
    /// @return A sentence for the player, empty after a success.
    [[nodiscard]] const std::string& room_error() const noexcept;

    /// @brief Leaves the server: sends `Logout` and closes once it has left.
    /// @details The state becomes Closed at once; keep running the context a little for the
    /// `Logout` to leave.
    void leave();

    /// @brief Where the session is.
    /// @return The state.
    [[nodiscard]] ClientState state() const noexcept;

    /// @brief Why the session failed or was lost.
    /// @return A sentence for the player, empty unless the state is Failed or Lost.
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
    /// to 4.5), each snapshot rebuilt from its baseline first: see ClientWorld.
    /// @return The world.
    [[nodiscard]] ClientWorld& world() noexcept;

    /// @copydoc world()
    [[nodiscard]] const ClientWorld& world() const noexcept;

    /// @brief The snapshots rebuilt from the `Snapshot` datagrams, as baselines of the next.
    /// @details A datagram whose baseline the client no longer has is dropped and not
    /// acknowledged, so the server encodes the next snapshots against another one.
    /// @return The decoder.
    [[nodiscard]] const SnapshotDecoder& snapshots() const noexcept;

    /// @brief The client's own ship, moved at once by each `Input` sent.
    /// @details Started from the position of the ship in the first snapshot that has it, and
    /// again when the player loses a life (the server moved it back to its spawn point) or
    /// gets another ship; stopped once the player is out. Each newer snapshot reconciles it:
    /// the ship goes back to the server's position once it applied the input the player
    /// record acknowledges, and the inputs after it are replayed (ShipPrediction::reconcile()).
    /// Each update() smooths the corrections (ShipPrediction::smooth()): the screen draws the
    /// ship at its drawn() position, so it answers the keys within a frame whatever the round
    /// trip time, and never jumps by more than a few pixels for a small correction.
    /// @return The prediction.
    [[nodiscard]] const ShipPrediction& prediction() const noexcept;

    /// @brief Number of `Input` datagrams sent.
    /// @return The count since the game state was reached.
    [[nodiscard]] std::uint64_t inputs_sent() const noexcept;

    /// @brief Sequence of the last `Input` sent (RFC section 4.2).
    /// @details The inputs are numbered from 0, one more for each, from 65535 back to 0: the
    /// low 16 bits of inputs_sent() before it is sent.
    /// @return The sequence, or nothing before the first `Input`.
    [[nodiscard]] std::optional<std::uint16_t> last_input() const noexcept;

    /// @brief Sequence of the last `Input` of the player the server applied, from the player
    /// record of its own player in the snapshots (RFC section 4.3).
    /// @details The inputs after it, up to last_input(), are those the server has not applied
    /// yet. It only moves forward: a late snapshot does not move it back.
    /// @return The sequence, or nothing until a snapshot says the server applied one.
    [[nodiscard]] std::optional<std::uint16_t> acknowledged_input() const noexcept;

    /// @brief Datagrams exchanged with the server, by direction and packet type.
    /// @details Counts every datagram sent to the server's UDP port, and every one received
    /// with the token of the session, since the login: the source of the network
    /// statistics of the client.
    /// @return The counters.
    [[nodiscard]] const aegis2d::net::TrafficCounters& traffic() const noexcept;

    /// @brief Round trip time to the server, with its jitter.
    /// @details A sample each time a datagram of the server acknowledges, for the first time,
    /// one sent to it (`Connect`, `Input`). It includes the time the server waits before its
    /// next datagram: in game, for the `Input` it acknowledges, at most the input interval,
    /// as a snapshot acknowledges the last one received. Next to traffic(), the source of
    /// the network statistics of the client.
    /// @return The estimate, without samples before the server's `Accept`.
    [[nodiscard]] const aegis2d::net::RttEstimator& rtt() const noexcept;

    /// @brief Loss, snapshot interval and jitter of the server's datagrams.
    /// @details Records every datagram received with the token of the session, since the
    /// login; NetworkStats::summary() gives the figures of the last NetworkStats::window.
    /// @return The statistics.
    [[nodiscard]] const NetworkStats& network_stats() const noexcept;

private:
    void fail(std::string reason);
    void on_connected();
    void on_message(std::span<const std::byte> message);
    void on_datagram(const aegis2d::net::Endpoint& from, std::span<const std::byte> datagram);
    void start_binding();
    void on_room_message(std::span<const std::byte> message);
    bool on_chat(std::span<const std::byte> message);
    void send_request(std::vector<std::byte> message);
    void send_connect(Clock::time_point now);
    void send_input(Clock::time_point now);
    void predict(const Snapshot& snapshot);
    void on_game_datagram(const aegis2d::net::PacketHeader& header,
                          aegis2d::net::BinaryReader& payload, Clock::time_point now);

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
    Clock::time_point last_room_list_;
    std::vector<RoomRecord> rooms_;
    std::string room_error_;
    RoomPlayers room_players_;
    ChatLog chat_;
    std::uint64_t connects_sent_ = 0;
    std::uint8_t held_ = 0;
    Clock::time_point next_input_;
    std::uint64_t inputs_sent_ = 0;
    // The last inputs sent, oldest first: each Input carries them all.
    std::vector<InputRecord> recent_inputs_;
    aegis2d::net::TrafficCounters traffic_;
    NetworkStats network_stats_;
    // Last valid datagram from the server, in game.
    Clock::time_point last_heard_;
    ClientWorld world_;
    SnapshotDecoder snapshots_;
    ShipPrediction prediction_;
    // Lives of the player when the prediction started: one less means a respawn.
    std::uint8_t predicted_lives_ = 0;
    // Tick of the last snapshot the prediction started from or was reconciled with.
    std::optional<std::uint32_t> reconciled_tick_;
    // Time of the last update(), to smooth the corrections of the prediction.
    std::optional<Clock::time_point> last_update_;
    // Messages handed to the connection, to close it once they have left.
    std::uint64_t sent_ = 0;
    bool closing_ = false;
    // Whether the player said it is ready, in its room.
    bool ready_ = false;
};

} // namespace rtype
