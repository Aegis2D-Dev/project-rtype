/// @file
/// @brief Sockets of the R-Type server: the TCP port of the logins and the UDP port of the game.
#pragma once
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

#include <rtype/chat.hpp>
#include <rtype/events.hpp>
#include <rtype/login.hpp>
#include <rtype/rate_limit.hpp>
#include <rtype/scoreboard.hpp>
#include <rtype/sessions.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/snapshot_history.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace rtype {

/// @brief The two ports the server listens on, and the TCP side of the sessions.
/// @details Binds both ports in its constructor, so a port already in use stops the server
/// at start. Each TCP connection must log in (RFC section 3.3): a valid `LoginRequest`
/// opens a session and gets `LoginAccepted`; a refused one gets `LoginRejected` and is
/// closed once it is sent; a connection that sends anything else first, or nothing within
/// login_timeout, is closed without a reply (section 6.5). Once logged in, a `ListRooms` gets
/// the `RoomList` of the rooms in use and a `CreateRoom` its `RoomCreated` or `RoomRejected`
/// (section 3.6); a `JoinRoom` gets `RoomJoined` or `JoinRejected`, a `LeaveRoom` moves the
/// session to the lobby and a `SetReady` sets whether it is ready (section 3.7); a `ChatSend`
/// whose text and rate pass (check_chat_text(), ChatRate) goes as a `ChatMessage` to every
/// session of the sender's room, or of the lobby, the sender included, and any other gets
/// `ChatRejected` with its reason (section 3.8); a `Logout` or
/// the closing of the connection ends the session; any other message is a protocol error that
/// ends it too. Whenever the sessions of a room or their ready states change, the clients of
/// that room that sent a message of the rooms and are not in game get its `RoomPlayers`, then
/// `GameStart` once every session of the room out of game is ready.
///
/// Each datagram is checked as RFC section 6.3 says (header, direction, source). A
/// `Connect` with an empty payload and the token of a session binds its source endpoint
/// to the session and is answered with `Accept`, again for each `Connect` from that
/// endpoint (section 3.4). Every other datagram must come from a bound endpoint with the
/// token of its session, and a payload as its type requires: one byte of actions for
/// `Input`, nothing for `Heartbeat` and `Disconnect`, a well-framed list for `Event`
/// (whose events are ignored, as only the server sends events). Datagrams that fail a
/// check are dropped without an answer and counted; the others are acknowledged in the
/// next datagram sent to the player. The most recent `Input` goes to on_input(); a
/// `Disconnect` ends the session like a `Logout`. A bound player that sends no valid
/// datagram for udp_timeout has lost its session (RFC section 5.5): it gets `Logout` with
/// the reason `TimedOut`, its connection is closed once it is sent, and it leaves the game.
///
/// Each session belongs to a room (Session::room): snapshots and events go to the players
/// of one room only, so the games of several rooms run side by side without seeing each
/// other.
///
/// The handling of each datagram and TCP message is contained: an exception is logged, the
/// client that sent the packet is kicked (`Logout` with the reason `ProtocolError`, then
/// `Kicked` for the game, RFC section 6.6), and the sockets go on serving the others.
///
/// Connections and datagrams are handled on the thread running the context; update() runs
/// the timeouts and removes closed connections. Create, use and destroy it on that thread,
/// or while the context is not running.
class ServerNetwork {
public:
    /// @brief Clock of the login and UDP timeouts.
    using Clock = std::chrono::steady_clock;

    /// @brief Called with a session that joins the game.
    using PlayerHandler = std::function<void(const Session& session)>;

    /// @brief Called with a session that leaves the game, and why: `Left` for a `Logout`, a
    /// `Disconnect` or a closed TCP connection, `TimedOut` for a silent player or one that
    /// acknowledges no event, `Kicked` for a protocol error.
    using LeaveHandler = std::function<void(const Session& session, LeaveReason reason)>;

    /// @brief Called with the actions a player holds and the sequence of the `Input` they
    /// come from, its most recent one.
    using InputHandler = std::function<void(const Session& session, aegis2d::ecs::PlayerInput input,
                                            std::uint16_t sequence)>;

    /// @brief Time a new connection has to send its `LoginRequest`.
    static constexpr std::chrono::seconds login_timeout{5};

    /// @brief Time a bound player may send no valid datagram before its session is lost
    /// (RFC section 5.5).
    static constexpr std::chrono::seconds udp_timeout{5};

    /// @brief Time between two lines of traffic and round trip time of each bound player in
    /// the log.
    static constexpr std::chrono::seconds traffic_report_period{10};

    /// @brief Listens on both ports, on every IPv4 interface.
    /// @param context Context that runs the sockets; it must outlive this object.
    /// @param tcp_port TCP port of the logins, or 0 for any free port.
    /// @param udp_port UDP port of the game, or 0 for any free port.
    /// @param sessions Sessions to fill, empty, with their rooms (SessionTable::set_rooms());
    /// tests give one with a seed.
    /// @throws aegis2d::net::SocketError If a port is already in use or cannot be bound.
    ServerNetwork(aegis2d::net::NetworkContext& context, std::uint16_t tcp_port,
                  std::uint16_t udp_port, SessionTable sessions = SessionTable{});

    ServerNetwork(const ServerNetwork&) = delete;
    ServerNetwork& operator=(const ServerNetwork&) = delete;
    ServerNetwork(ServerNetwork&&) = delete;
    ServerNetwork& operator=(ServerNetwork&&) = delete;
    ~ServerNetwork() = default;

    /// @brief Sets what to do when a player joins or leaves the game.
    /// @details A session joins once its UDP endpoint is bound (its first `Accept`), so a
    /// client that cannot reach the UDP port never appears in the game. It leaves when it
    /// ends (`Logout`, `Disconnect`, TCP closed, UDP timeout, protocol error), if it had joined.
    /// @param joined Called with the session, its endpoint bound.
    /// @param left Called with the session and why it ended, once it is forgotten.
    void on_players(PlayerHandler joined, LeaveHandler left);

    /// @brief Sets what to do with the inputs of the players.
    /// @details Called for each input of a valid `Input` datagram from a bound endpoint that
    /// is more recent than the last one passed on (by its sequence, InputRecord), oldest
    /// first: an input whose datagram was lost is passed on when one of the next three
    /// arrives; those passed on already, late ones and malformed datagrams are not (RFC
    /// section 4.2).
    /// @param input Called with the session, the actions it holds and their sequence.
    void on_input(InputHandler input);

    /// @brief Sets from which size the payloads of snapshots and events are compressed.
    /// @details A payload of at least @p threshold bytes is sent compressed with LZ4 when that
    /// makes it smaller (`aegis2d/net/compression.hpp`, RFC section 2.6). Whatever this
    /// setting, compressed datagrams received are decompressed.
    /// @param threshold Size in bytes, or `std::nullopt` (the default) to never compress.
    void set_compression(std::optional<std::size_t> threshold) noexcept;

    /// @brief Sends a message of the server to every session logged in (`ChatMessage` with
    /// server_player and server_name, RFC section 3.8).
    /// @param text Text to send, checked as a chat text.
    /// @return The number of sessions it was sent to, or ChatRejection::TooLong or BadText.
    std::variant<std::size_t, ChatRejection> say(std::string_view text);

    /// @brief Closes a room: each of its sessions gets `Logout` with `RoomClosed` and ends;
    /// the others of the game are told it was kicked.
    /// @param room Number of the room.
    /// @return The number of sessions ended, 0 if the room was empty or not in use.
    std::size_t close_room(std::uint32_t room);

    /// @brief Sets the board a `ListScores` reads (RFC section 3.9).
    /// @param board The best scores, which must outlive this object, or nullptr to answer
    /// with none.
    void set_scoreboard(const Scoreboard* board) noexcept;

    /// @brief Sets the waves files a room may be created with (SessionTable::set_waves()).
    /// @param names Their names, without `.json`.
    void set_waves(std::vector<std::string> names);

    /// @brief From which size payloads are compressed.
    /// @return The threshold set by set_compression(), or `std::nullopt`.
    [[nodiscard]] std::optional<std::size_t> compression() const noexcept;

    /// @brief Sends a snapshot to every player of a room whose endpoint is bound.
    /// @details Each payload goes in its own `Snapshot` datagram, whose header takes the
    /// next sequence of the player's channel and acknowledges its datagrams (RFC 5.1).
    /// @param payloads Payloads from write_snapshot(), header excluded.
    /// @param room Room of the game the snapshot comes from (Session::room).
    /// @param snapshot The snapshot they encode, to keep in each player's snapshots() with
    /// the sequences of its datagrams; none by default.
    void send_snapshot(std::span<const std::vector<std::byte>> payloads, std::uint32_t room,
                       const std::shared_ptr<const Snapshot>& snapshot = nullptr);

    /// @brief Sends a snapshot to every player of a room whose endpoint is bound, encoded
    /// against the latest snapshot that player acknowledged.
    /// @details write_delta_snapshot() encodes it against snapshots()'s latest
    /// acknowledged one, or whole if there is none; the payloads go as with the other
    /// overload, and the snapshot is kept in the player's snapshots().
    /// @param snapshot The snapshot to send; nothing is sent if it is `nullptr`.
    /// @param room Room of the game the snapshot comes from (Session::room).
    /// @throws std::invalid_argument if a field does not fit, as write_delta_snapshot().
    void send_snapshot(const std::shared_ptr<const Snapshot>& snapshot, std::uint32_t room);

    /// @brief Number of `Snapshot` datagrams sent, all players together.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t snapshots_sent() const noexcept;

    /// @brief Sends gameplay events, reliably, to the players of a room whose endpoint is
    /// bound.
    /// @details Each event is queued on the player's channel, which numbers it and resends
    /// it until it is acknowledged (RFC section 5.3): the events leave at once in `Event`
    /// datagrams, the resends in update(). A player who leaves
    /// `net::max_events_in_flight` events unacknowledged is lost (RFC section 6.2): its
    /// session ends.
    /// @param events Events from write_event(), in order.
    /// @param room Room of the game the events come from (Session::room).
    /// @param token Only to this session of the room, for a player who joins (RFC section
    /// 4.6); to every bound player of the room by default.
    void send_events(std::span<const std::vector<std::byte>> events, std::uint32_t room,
                     std::optional<std::uint32_t> token = std::nullopt);

    /// @brief Sends gameplay events, reliably, to every bound player of a room but one, as
    /// send_events().
    /// @details For the `PlayerJoined` of a player who joins, which the others get (RFC
    /// section 4.6).
    /// @param events Events from write_event(), in order.
    /// @param room Room of the game the events come from (Session::room).
    /// @param token Session that does not get them.
    void send_events_to_others(std::span<const std::vector<std::byte>> events, std::uint32_t room,
                               std::uint32_t token);

    /// @brief Number of `Event` datagrams sent, first sends and resends, all players together.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t event_datagrams_sent() const noexcept;

    /// @brief Ends the sessions of the bound players silent for udp_timeout, closes the
    /// connections that did not log in within login_timeout and those whose rejection or
    /// `Logout` is sent, then forgets the closed ones; resends the events that are due.
    /// @details Call it regularly, a few times per second at least.
    /// @param now Current time.
    void update(Clock::time_point now);

    /// @brief Sets the limits on the traffic of one address (RFC section 6.6).
    /// @details Datagrams past RateLimits::datagrams_per_second in a second are dropped
    /// before any other check, and a connection past RateLimits::connections_per_address
    /// open ones is closed at once. Defaults: 300 datagrams per second, 8 connections.
    /// @param limits New limits; the counts so far are forgotten.
    void set_limits(RateLimits limits);

    /// @brief Starts a clean stop of the server (RFC section 3.2).
    /// @details Stops accepting connections, sends `Logout` with the reason `Shutdown` to
    /// every logged-in player and ends its session, and closes the connections that did not
    /// log in. Each connection closes once its `Logout` has left: keep running the context
    /// and update() until idle().
    void shutdown();

    /// @brief Tells whether every connection is closed and forgotten, after shutdown().
    /// @return `true` when no TCP connection is left.
    [[nodiscard]] bool idle() const noexcept;

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

    /// @brief Number of UDP datagrams dropped because their address sent too many.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t rate_limited() const noexcept;

    /// @brief Number of TCP connections closed at once, their address having too many open.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t refused_connections() const noexcept;

    /// @brief Number of UDP datagrams dropped for their header, direction or source.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t dropped_datagrams() const noexcept;

    /// @brief Number of valid datagrams from bound endpoints, other than `Connect`.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t game_datagrams() const noexcept;

    /// @brief Number of sessions ended because their player sent nothing for udp_timeout.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t timeouts() const noexcept;

    /// @brief Number of datagrams and TCP messages whose handling threw.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t errors() const noexcept;

    /// @brief Datagrams exchanged with a bound player, by direction and packet type.
    /// @details Counts the datagrams sent to the player (`Accept`, `Snapshot`, `Event`) and
    /// the valid ones received from it, since its endpoint was bound; update() logs them
    /// every traffic_report_period.
    /// @param token Session of the player.
    /// @return Its counters, or nothing if the session is not bound or has ended.
    [[nodiscard]] const aegis2d::net::TrafficCounters* traffic(std::uint32_t token) const;

    /// @brief Round trip time to a bound player, with its jitter.
    /// @details A sample each time a datagram of the player acknowledges, for the first
    /// time, one sent to it (`Accept`, `Snapshot`, `Event`). It includes the time the client
    /// waits before its next datagram: in game, at most its input interval (about 17 ms).
    /// update() logs it every traffic_report_period.
    /// @param token Session of the player.
    /// @return Its estimate, or nothing if the session is not bound or has ended.
    [[nodiscard]] const aegis2d::net::RttEstimator* rtt(std::uint32_t token) const;

    /// @brief The last snapshots sent to a player and the ones it acknowledged.
    /// @details Every valid datagram of the player acknowledges the server's datagrams it
    /// received (`ack`, `ack_bits`): a snapshot is acknowledged once all its datagrams are.
    /// @param token Session of the player.
    /// @return Its history, or nothing if the session is not bound or has ended.
    [[nodiscard]] const SnapshotHistory* snapshots(std::uint32_t token) const;

private:
    struct Client {
        aegis2d::net::TcpConnection connection;
        Clock::time_point accepted;
        std::optional<std::uint32_t> token;
        std::uint64_t sent = 0;
        bool close_when_sent = false;
        bool closed = false;
        // Whether it sent a message of the rooms, and so gets RoomPlayers and GameStart.
        bool rooms = false;
        ChatRate chat;
    };

    void on_accept(aegis2d::net::TcpConnection connection);
    void on_message(std::uint64_t id, std::span<const std::byte> message);
    void on_login(Client& client, std::span<const std::byte> message);
    bool on_room_message(Client& client, std::span<const std::byte> message);
    bool on_chat(Client& client, std::span<const std::byte> message);
    void on_join(Client& client, std::uint8_t room);
    void announce_room(std::uint32_t room);
    void on_close(std::uint64_t id, aegis2d::net::TcpCloseReason reason);
    void on_datagram(const aegis2d::net::Endpoint& from, std::span<const std::byte> datagram);
    void on_connect(const aegis2d::net::Endpoint& from, std::span<const std::byte> datagram,
                    const aegis2d::net::PacketHeader& header,
                    const aegis2d::net::BinaryReader& payload);
    void drop(const aegis2d::net::Endpoint& from, std::span<const std::byte> datagram,
              std::string_view reason);
    void on_game_datagram(const Session& session, const aegis2d::net::Endpoint& from,
                          std::span<const std::byte> datagram,
                          const aegis2d::net::PacketHeader& header,
                          aegis2d::net::BinaryReader& payload);
    void disconnect(std::uint32_t token, LeaveReason reason);
    void end_session(std::uint32_t token, LeaveReason reason);
    void queue_events(std::span<const std::vector<std::byte>> events,
                      const std::function<bool(const Session& session)>& receives);
    void send(Client& client, std::span<const std::byte> message);
    void close(Client& client, std::string_view reason);
    void flush_events(Clock::time_point now);
    void time_out(std::uint32_t token);
    void kick(std::uint32_t token);
    void kick_client(std::uint64_t id);
    void end_with_logout(std::uint32_t token, LogoutReason logout, LeaveReason leave);
    void report_traffic(Clock::time_point now);
    // Sends the payloads encode() gives for the history of each bound player of the room.
    void send_snapshot(
        const std::function<std::vector<std::vector<std::byte>>(const SnapshotHistory&)>& encode,
        std::uint32_t room, const std::shared_ptr<const Snapshot>& snapshot);

    aegis2d::net::TcpListener listener_;
    aegis2d::net::UdpSocket socket_;
    SessionTable sessions_;
    std::unordered_map<std::uint64_t, Client> clients_;
    // What the server keeps of each bound session's datagrams, by token.
    struct Peer {
        aegis2d::net::ReliableChannel channel;
        // Sequence of the last Input passed on (InputRecord::sequence).
        std::optional<std::uint16_t> last_input;
        // When its last valid datagram arrived, for the UDP timeout.
        Clock::time_point heard = Clock::now();
        aegis2d::net::TrafficCounters traffic;
        SnapshotHistory snapshots;
    };
    std::unordered_map<std::uint32_t, Peer> peers_;
    PlayerHandler joined_;
    LeaveHandler left_;
    InputHandler input_;
    std::uint64_t connections_ = 0;
    std::uint64_t dropped_ = 0;
    std::uint64_t game_datagrams_ = 0;
    std::uint64_t snapshots_sent_ = 0;
    std::uint64_t event_datagrams_sent_ = 0;
    std::uint64_t timeouts_ = 0;
    std::uint64_t errors_ = 0;
    RateLimits limits_;
    RateLimiter limiter_{limits_.datagrams_per_second};
    const Scoreboard* scores_ = nullptr;
    DropReport drops_;
    std::uint64_t rate_limited_ = 0;
    std::uint64_t refused_connections_ = 0;
    std::optional<std::size_t> compression_;
    std::optional<Clock::time_point> next_traffic_report_;
};

} // namespace rtype
