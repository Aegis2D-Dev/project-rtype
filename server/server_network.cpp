#include <aegis2d/core/log.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/binary.hpp>
#include <aegis2d/net/compression.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/reliability.hpp>
#include <aegis2d/net/rtt.hpp>
#include <aegis2d/net/sequence.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/traffic.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <rtype/chat.hpp>
#include <rtype/delta_snapshot.hpp>
#include <rtype/events.hpp>
#include <rtype/input.hpp>
#include <rtype/login.hpp>
#include <rtype/rate_limit.hpp>
#include <rtype/rooms.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/snapshot_history.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;
namespace core = aegis2d::core;
namespace ecs = aegis2d::ecs;

namespace {

// "round trip 203 ms (jitter 4 ms)", in whole milliseconds, or that none was measured yet.
std::string describe_rtt(const net::RttEstimator& rtt) {
    if (!rtt.has_estimate()) {
        return "no round trip measured yet";
    }
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    return std::format("round trip {} ms (jitter {} ms)",
                       duration_cast<milliseconds>(rtt.smoothed()).count(),
                       duration_cast<milliseconds>(rtt.jitter()).count());
}

// Runs the handling of one packet; an exception is logged and reported, never let out, so a
// client can only break its own session (RFC section 6.6).
template <typename Handling>
bool contained(std::string_view what, Handling&& handling) noexcept {
    try {
        std::forward<Handling>(handling)();
        return true;
    } catch (const std::exception& error) {
        try {
            core::log_error("{}: {}", what, error.what());
        } catch (...) { // NOLINT(bugprone-empty-catch): nothing left to report it with
        }
    } catch (...) {
        try {
            core::log_error("{}: unknown exception", what);
        } catch (...) { // NOLINT(bugprone-empty-catch): nothing left to report it with
        }
    }
    return false;
}

// A datagram written by ReliableChannel, its payload compressed when that pays (RFC 2.6).
net::BinaryWriter compressed(std::span<const std::byte> datagram,
                             std::optional<std::size_t> threshold) {
    net::BinaryReader reader{datagram};
    const std::variant<net::PacketHeader, net::HeaderError> read = net::read_header(reader);
    net::BinaryWriter writer;
    if (const auto* const header = std::get_if<net::PacketHeader>(&read)) {
        net::write_datagram(writer, *header, datagram.subspan(net::PacketHeader::size), threshold);
    } else {
        writer.write_bytes(datagram);
    }
    return writer;
}

// The bytes of a payload not read yet.
std::span<const std::byte> rest_of(net::BinaryReader& payload) {
    const std::optional<std::span<const std::byte>> rest = payload.read_bytes(payload.remaining());
    return rest.value_or(std::span<const std::byte>{});
}

} // namespace

ServerNetwork::ServerNetwork(net::NetworkContext& context, std::uint16_t tcp_port,
                             std::uint16_t udp_port, SessionTable sessions)
    : listener_{context, tcp_port}, socket_{context, udp_port}, sessions_{std::move(sessions)} {
    listener_.start_accepting([this](net::TcpConnection connection) {
        static_cast<void>(contained("TCP: accepting a connection failed",
                                    [&] { on_accept(std::move(connection)); }));
    });
    socket_.start_receiving([this](const net::Endpoint& from, std::span<const std::byte> datagram) {
        if (!contained("UDP: handling a datagram failed", [&] { on_datagram(from, datagram); })) {
            ++errors_;
            static_cast<void>(contained("UDP: kicking a player failed", [&] {
                if (const Session* const session = sessions_.find(from)) {
                    kick(session->token);
                }
            }));
        }
    });
}

// RFC section 6.3: header, direction, source, payload, then sequence.
void ServerNetwork::on_datagram(const net::Endpoint& from, std::span<const std::byte> datagram) {
    // RFC section 6.6: an address sending more than a client ever needs is cut off before
    // anything else is read, so a flood costs one lookup per datagram.
    if (!limiter_.allow(from, Clock::now())) {
        ++rate_limited_;
        drop(from, datagram, "rate limit");
        return;
    }
    net::BinaryReader reader{datagram};
    const std::variant<net::PacketHeader, net::HeaderError> read = net::read_header(reader);
    if (const auto* const error = std::get_if<net::HeaderError>(&read)) {
        drop(from, datagram, net::to_string(*error));
        return;
    }
    const auto& header = *std::get_if<net::PacketHeader>(&read);
    if (header.type == net::PacketType::Accept || header.type == net::PacketType::Snapshot) {
        drop(from, datagram, "wrong direction");
        return;
    }
    // RFC section 2.6: a compressed payload is decompressed before anything reads it.
    std::vector<std::byte> decompressed;
    if (header.compressed) {
        std::optional<std::vector<std::byte>> payload = net::read_payload(header, reader);
        if (!payload) {
            drop(from, datagram, "malformed compressed payload");
            return;
        }
        decompressed = std::move(payload).value_or(std::vector<std::byte>{});
        reader = net::BinaryReader{decompressed};
    }
    if (header.type == net::PacketType::Connect) {
        on_connect(from, datagram, header, reader);
        return;
    }
    const Session* const session = sessions_.find(from);
    if (session == nullptr) {
        drop(from, datagram, "endpoint not bound");
        return;
    }
    if (session->token != header.session) {
        drop(from, datagram, "wrong session");
        return;
    }
    on_game_datagram(*session, from, datagram, header, reader);
}

// Datagrams of a bound session: its payload is checked whole before anything changes, then
// its sequence and acknowledgements are recorded (RFC section 5.1).
void ServerNetwork::on_game_datagram(const Session& session, const net::Endpoint& from,
                                     std::span<const std::byte> datagram,
                                     const net::PacketHeader& header, net::BinaryReader& payload) {
    Peer& peer = peers_[session.token];
    switch (header.type) {
    case net::PacketType::Input: {
        // The newest input and up to 3 before it, each one byte of held actions with bits 5
        // to 7 clear (RFC section 4.2).
        const std::optional<std::vector<InputRecord>> read = read_inputs(rest_of(payload));
        if (!read) {
            drop(from, datagram, "malformed Input");
            return;
        }
        peer.heard = Clock::now();
        peer.channel.receive(header, peer.heard);
        peer.snapshots.acknowledge(header.ack, header.ack_bits);
        peer.traffic.record(net::Direction::Received, header.type, datagram.size(), peer.heard);
        ++game_datagrams_;
        // Oldest first, each input not applied yet, by its own sequence: those of a lost
        // datagram arrive with the next ones, those already applied are ignored.
        for (const InputRecord& input : read.value_or(std::vector<InputRecord>{})) {
            if (const std::optional<std::uint16_t> last = peer.last_input;
                last && !net::sequence_greater_than(input.sequence, *last)) {
                continue;
            }
            peer.last_input = input.sequence;
            if (input_) {
                input_(session, ecs::PlayerInput{.held = input.actions}, input.sequence);
            }
        }
        return;
    }
    case net::PacketType::Event: {
        // Only the server sends events in version 1: their framing is checked, they are
        // acknowledged and ignored (RFC section 6.4).
        std::vector<std::vector<std::byte>> events;
        const auto now = Clock::now();
        if (!peer.channel.read_events(header, payload, events, now)) {
            drop(from, datagram, "malformed Event");
            return;
        }
        peer.heard = now;
        peer.snapshots.acknowledge(header.ack, header.ack_bits);
        peer.traffic.record(net::Direction::Received, header.type, datagram.size(), peer.heard);
        ++game_datagrams_;
        return;
    }
    case net::PacketType::Heartbeat:
    case net::PacketType::Disconnect:
        if (!payload.at_end()) {
            drop(from, datagram,
                 header.type == net::PacketType::Heartbeat ? "malformed Heartbeat"
                                                           : "malformed Disconnect");
            return;
        }
        peer.heard = Clock::now();
        peer.channel.receive(header, peer.heard);
        peer.snapshots.acknowledge(header.ack, header.ack_bits);
        peer.traffic.record(net::Direction::Received, header.type, datagram.size(), peer.heard);
        ++game_datagrams_;
        if (header.type == net::PacketType::Disconnect) {
            // The client leaves (RFC section 5.5): its session ends, TCP connection included.
            core::log_info("UDP: player {} disconnected", session.player);
            disconnect(session.token, LeaveReason::Left);
        }
        return;
    case net::PacketType::Connect:
    case net::PacketType::Accept:
    case net::PacketType::Snapshot:
        break;
    }
}

void ServerNetwork::on_connect(const net::Endpoint& from, std::span<const std::byte> datagram,
                               const net::PacketHeader& header, const net::BinaryReader& payload) {
    if (!payload.at_end()) {
        drop(from, datagram, "malformed Connect");
        return;
    }
    switch (sessions_.bind(header.session, from)) {
    case BindResult::UnknownSession:
        drop(from, datagram, "unknown session");
        return;
    case BindResult::Taken:
        drop(from, datagram, "session or endpoint bound already");
        return;
    case BindResult::NoRoom:
        drop(from, datagram, "session in the lobby");
        return;
    case BindResult::Bound: {
        const Session& session = *sessions_.find(header.session);
        core::log_info("UDP: player {} of room {} plays from {}", session.player, session.room,
                       from.to_string());
        const std::uint32_t room = session.room;
        if (joined_) {
            joined_(session);
        }
        announce_room(room);
        break;
    }
    case BindResult::AlreadyBound:
        break;
    }
    // The first Accept may be lost: each Connect from the bound endpoint gets one.
    Peer& peer = peers_[header.session];
    peer.heard = Clock::now();
    peer.traffic.record(net::Direction::Received, header.type, datagram.size(), peer.heard);
    net::ReliableChannel& channel = peer.channel;
    channel.receive(header, peer.heard);
    peer.snapshots.acknowledge(header.ack, header.ack_bits);
    net::BinaryWriter writer;
    net::write_header(writer,
                      channel.next_header(net::PacketType::Accept, header.session, peer.heard));
    socket_.send_to(from, writer.data());
    peer.traffic.record(net::Direction::Sent, net::PacketType::Accept, writer.data().size(),
                        peer.heard);
}

// Never answered (RFC section 6.1), and only logged at the debug level (section 6.7).
void ServerNetwork::drop(const net::Endpoint& from, std::span<const std::byte> datagram,
                         std::string_view reason) {
    ++dropped_;
    core::log_debug("UDP: dropped a {}-byte datagram from {}: {}", datagram.size(),
                    from.to_string(), reason);
    if (drops_.count(from, reason, Clock::now()) && reason == "rate limit") {
        core::log_info("UDP: {} sends more than {} datagrams per second: dropping the rest",
                       from.address(), limits_.datagrams_per_second);
    }
}

void ServerNetwork::set_limits(RateLimits limits) {
    limits_ = limits;
    limiter_ = RateLimiter{limits.datagrams_per_second};
}

std::uint64_t ServerNetwork::rate_limited() const noexcept {
    return rate_limited_;
}

std::uint64_t ServerNetwork::refused_connections() const noexcept {
    return refused_connections_;
}

void ServerNetwork::on_players(PlayerHandler joined, LeaveHandler left) {
    joined_ = std::move(joined);
    left_ = std::move(left);
}

void ServerNetwork::send_snapshot(std::span<const std::vector<std::byte>> payloads,
                                  std::uint32_t room,
                                  const std::shared_ptr<const Snapshot>& snapshot) {
    send_snapshot(
        [payloads](const SnapshotHistory&) {
            return std::vector<std::vector<std::byte>>{payloads.begin(), payloads.end()};
        },
        room, snapshot);
}

void ServerNetwork::send_snapshot(const std::shared_ptr<const Snapshot>& snapshot,
                                  std::uint32_t room) {
    if (!snapshot) {
        return;
    }
    send_snapshot(
        [&snapshot](const SnapshotHistory& history) {
            const Snapshot* baseline = history.latest_acknowledged();
            if (baseline != nullptr && baseline->tick >= snapshot->tick) {
                baseline = nullptr;
            }
            return write_delta_snapshot(*snapshot, baseline);
        },
        room, snapshot);
}

void ServerNetwork::send_snapshot(
    const std::function<std::vector<std::vector<std::byte>>(const SnapshotHistory&)>& encode,
    std::uint32_t room, const std::shared_ptr<const Snapshot>& snapshot) {
    const auto now = Clock::now();
    for (auto& [token, peer] : peers_) {
        const Session* const session = sessions_.find(token);
        if (session == nullptr || session->room != room) {
            continue;
        }
        if (!session->endpoint.has_value()) {
            continue;
        }
        const net::Endpoint endpoint = session->endpoint.value_or(net::Endpoint{});
        std::vector<std::uint16_t> sequences;
        for (const std::vector<std::byte>& payload : encode(peer.snapshots)) {
            net::BinaryWriter writer;
            const net::PacketHeader header =
                peer.channel.next_header(net::PacketType::Snapshot, token, now);
            sequences.push_back(header.sequence);
            net::write_datagram(writer, header, payload, compression_);
            socket_.send_to(endpoint, writer.data());
            peer.traffic.record(net::Direction::Sent, net::PacketType::Snapshot,
                                writer.data().size(), now);
            ++snapshots_sent_;
        }
        if (snapshot) {
            peer.snapshots.sent(snapshot, sequences);
        }
    }
}

void ServerNetwork::set_compression(std::optional<std::size_t> threshold) noexcept {
    compression_ = threshold;
}

void ServerNetwork::set_waves(std::vector<std::string> names) {
    sessions_.set_waves(std::move(names));
}

std::optional<std::size_t> ServerNetwork::compression() const noexcept {
    return compression_;
}

std::uint64_t ServerNetwork::snapshots_sent() const noexcept {
    return snapshots_sent_;
}

void ServerNetwork::send_events(std::span<const std::vector<std::byte>> events, std::uint32_t room,
                                std::optional<std::uint32_t> token) {
    queue_events(events, [&](const Session& session) {
        return session.room == room && (!token || *token == session.token);
    });
}

void ServerNetwork::send_events_to_others(std::span<const std::vector<std::byte>> events,
                                          std::uint32_t room, std::uint32_t token) {
    queue_events(events, [&](const Session& session) {
        return session.room == room && session.token != token;
    });
}

void ServerNetwork::queue_events(std::span<const std::vector<std::byte>> events,
                                 const std::function<bool(const Session& session)>& receives) {
    std::vector<std::uint32_t> lost;
    for (auto& [peer_token, peer] : peers_) {
        const Session* const session = sessions_.find(peer_token);
        if (session == nullptr || !session->endpoint || !receives(*session)) {
            continue;
        }
        for (const std::vector<std::byte>& event : events) {
            if (!peer.channel.send(event)) {
                lost.push_back(peer_token);
                break;
            }
        }
    }
    // Ended once the loop is over: ending a session erases its peer.
    for (const std::uint32_t lost_token : lost) {
        if (const Session* const session = sessions_.find(lost_token)) {
            core::log_warn("UDP: player {} acknowledges no event: session lost", session->player);
        }
        disconnect(lost_token, LeaveReason::TimedOut);
    }
    flush_events(Clock::now());
}

std::uint64_t ServerNetwork::event_datagrams_sent() const noexcept {
    return event_datagrams_sent_;
}

// The events due of each bound player, new or to resend, in as many datagrams as they need.
void ServerNetwork::flush_events(Clock::time_point now) {
    for (auto& [token, peer] : peers_) {
        const Session* const session = sessions_.find(token);
        if (session == nullptr || !session->endpoint) {
            continue;
        }
        const net::Endpoint endpoint = session->endpoint.value_or(net::Endpoint{});
        while (peer.channel.has_due_events(now)) {
            net::BinaryWriter events;
            if (peer.channel.write_events(events, token, now) == 0) {
                break;
            }
            const net::BinaryWriter writer = compressed(events.data(), compression_);
            socket_.send_to(endpoint, writer.data());
            peer.traffic.record(net::Direction::Sent, net::PacketType::Event, writer.data().size(),
                                now);
            ++event_datagrams_sent_;
        }
    }
}

void ServerNetwork::on_input(InputHandler input) {
    input_ = std::move(input);
}

void ServerNetwork::disconnect(std::uint32_t token, LeaveReason reason) {
    for (auto& [id, client] : clients_) {
        if (client.token == token && !client.closed) {
            close(client, {});
        }
    }
    end_session(token, reason);
}

// Forgotten first, so a left handler that throws never leaves a session behind.
void ServerNetwork::end_session(std::uint32_t token, LeaveReason reason) {
    std::optional<Session> left;
    std::uint32_t room = no_room;
    if (const Session* const session = sessions_.find(token); session != nullptr) {
        room = session->room;
        if (session->endpoint) {
            left = *session;
        }
    }
    sessions_.end(token);
    peers_.erase(token);
    announce_room(room);
    if (left && left_) {
        left_(*left, reason);
    }
}

void ServerNetwork::on_accept(net::TcpConnection connection) {
    // RFC section 6.6: one address cannot hold more than its share of connections.
    const net::Endpoint address = address_of(connection.remote_endpoint());
    const auto open = std::ranges::count_if(clients_, [&address](const auto& entry) {
        return !entry.second.closed &&
               address_of(entry.second.connection.remote_endpoint()) == address;
    });
    if (std::cmp_greater_equal(open, limits_.connections_per_address)) {
        ++refused_connections_;
        core::log_info("TCP: closed {}: {} connections from {} are open already",
                       connection.remote_endpoint().to_string(), open, address.address());
        return; // the connection closes as it goes out of scope
    }
    const std::uint64_t id = connections_++;
    Client& client = clients_
                         .emplace(id, Client{.connection = std::move(connection),
                                             .accepted = Clock::now(),
                                             .token = std::nullopt,
                                             .sent = 0,
                                             .close_when_sent = false,
                                             .closed = false,
                                             .rooms = false,
                                             .chat = ChatRate{}})
                         .first->second;
    // The handlers find the client by id: it may be gone when a late handler runs.
    client.connection.start(
        [this, id](std::span<const std::byte> message) {
            if (!contained("TCP: handling a message failed", [&] { on_message(id, message); })) {
                ++errors_;
                static_cast<void>(
                    contained("TCP: kicking a player failed", [&] { kick_client(id); }));
            }
        },
        [this, id](net::TcpCloseReason reason) {
            if (!contained("TCP: handling a closed connection failed",
                           [&] { on_close(id, reason); })) {
                ++errors_;
            }
        });
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
    // Logged in: the messages of the rooms are answered, a Logout ends the session (RFC
    // section 3.3).
    if (on_room_message(client, message) || on_chat(client, message)) {
        return;
    }
    const Session* const session = sessions_.find(*client.token);
    const unsigned player = session != nullptr ? session->player : 0U;
    // Anything but a Logout is a protocol error: the player is kicked.
    const bool logged_out = read_logout(message).has_value();
    end_session(*client.token, logged_out ? LeaveReason::Left : LeaveReason::Kicked);
    if (logged_out) {
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
    core::log_info("TCP: {} logged in as player {} of room {} ({})",
                   client.connection.remote_endpoint().to_string(), session.player, session.room,
                   session.name);
    send(client, write_login_accepted(LoginAccepted{
                     .session = session.token,
                     .player = session.player,
                     .udp_port = socket_.local_endpoint().port(),
                 }));
    announce_room(session.room);
}

// RFC sections 3.6 and 3.7: the messages of the rooms, after the login; false for any other.
bool ServerNetwork::on_room_message(Client& client, std::span<const std::byte> message) {
    const std::uint32_t token = client.token.value_or(0);
    if (read_list_rooms(message)) {
        client.rooms = true;
        send(client, write_room_list(sessions_.list_rooms()));
        return true;
    }
    if (const std::optional<CreateRoomRequest> request = read_create_room(message)) {
        client.rooms = true;
        const std::variant<std::uint8_t, RoomRejection> created = sessions_.create_room(*request);
        if (const auto* const room = std::get_if<std::uint8_t>(&created)) {
            // The name passed valid_player_name(): no control character reaches the log.
            core::log_info("TCP: session {:08x} created room {} ({}, {} players)", token, *room,
                           request->name, request->max_players);
            send(client, write_room_created(*room));
        } else if (const auto* const rejection = std::get_if<RoomRejection>(&created)) {
            core::log_info("TCP: session {:08x} could not create a room: {}", token,
                           to_string(*rejection));
            send(client, write_room_rejected(*rejection));
        }
        return true;
    }
    if (const std::optional<std::uint8_t> room = read_join_room(message)) {
        client.rooms = true;
        on_join(client, *room);
        return true;
    }
    if (read_leave_room(message)) {
        client.rooms = true;
        if (const std::optional<std::uint32_t> left = sessions_.leave(token)) {
            core::log_info("TCP: session {:08x} left room {} for the lobby", token, *left);
            announce_room(*left);
        }
        return true;
    }
    if (const std::optional<bool> ready = read_set_ready(message)) {
        client.rooms = true;
        const Session* const session = sessions_.find(token);
        if (session != nullptr && sessions_.set_ready(token, *ready)) {
            announce_room(session->room);
        }
        return true;
    }
    return false;
}

// RFC section 3.8: a text checked, then relayed to the sessions of the sender's room (or of
// the lobby), the sender included; false for any other message.
bool ServerNetwork::on_chat(Client& client, std::span<const std::byte> message) {
    const std::optional<std::string> text = read_chat_send(message);
    if (!text) {
        return false;
    }
    const Session* const sender = sessions_.find(client.token.value_or(0));
    if (sender == nullptr) {
        return true;
    }
    std::optional<ChatRejection> rejection = check_chat_text(*text);
    if (!rejection && !client.chat.allow(Clock::now())) {
        rejection = ChatRejection::TooFast;
    }
    if (rejection) {
        // Debug only, like dropped datagrams: a flood must not become a flood of log lines.
        core::log_debug("TCP: chat of player {} refused: {}", sender->player,
                        to_string(*rejection));
        send(client, write_chat_rejected(*rejection));
        return true;
    }
    const std::uint32_t room = sender->room;
    const std::vector<std::byte> relayed = write_chat_message(ChatMessage{
        .player = room == no_room ? lobby_player : sender->player,
        .name = sender->name,
        .text = *text,
    });
    for (auto& [id, other] : clients_) {
        if (!other.token || other.closed || other.close_when_sent) {
            continue;
        }
        const Session* const session = sessions_.find(*other.token);
        if (session != nullptr && session->room == room) {
            send(other, relayed);
        }
    }
    return true;
}

// RFC section 3.7: the session leaves its room, if it was in one, for the room it joins.
void ServerNetwork::on_join(Client& client, std::uint8_t room) {
    const std::uint32_t token = client.token.value_or(0);
    const Session* const session = sessions_.find(token);
    const std::uint32_t old = session != nullptr ? session->room : no_room;
    const std::variant<std::uint8_t, JoinRejection> joined = sessions_.join(token, room);
    if (const auto* const rejection = std::get_if<JoinRejection>(&joined)) {
        core::log_info("TCP: session {:08x} could not join room {}: {}", token, room,
                       to_string(*rejection));
        send(client, write_join_rejected(*rejection));
        return;
    }
    if (const auto* const player = std::get_if<std::uint8_t>(&joined)) {
        core::log_info("TCP: session {:08x} joined room {} as player {}", token, room, *player);
        send(client, write_room_joined(room, *player));
    }
    if (old != room) {
        announce_room(old);
    }
    announce_room(room);
}

// RFC section 3.7: the players of a room and whether each is ready, to the clients of the
// room that use the rooms and are not in game, then GameStart once all those are ready.
void ServerNetwork::announce_room(std::uint32_t room) {
    if (room == no_room) {
        return;
    }
    RoomPlayers players{.room = static_cast<std::uint8_t>(room), .players = {}};
    bool waiting = false;
    bool ready = true;
    for (const Session& member : sessions_.members(room)) {
        players.players.push_back(
            RoomPlayer{.player = member.player, .name = member.name, .ready = member.ready});
        if (!member.endpoint) {
            waiting = true;
            ready = ready && member.ready;
        }
    }
    const std::vector<std::byte> list = write_room_players(players);
    for (auto& [id, client] : clients_) {
        if (!client.rooms || !client.token || client.closed || client.close_when_sent) {
            continue;
        }
        const Session* const session = sessions_.find(*client.token);
        if (session == nullptr || session->room != room || session->endpoint) {
            continue;
        }
        send(client, list);
        if (waiting && ready) {
            send(client, write_game_start());
        }
    }
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
        end_session(*client.token, LeaveReason::Left);
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
    // Before the timeouts, so that a player who times out gets its last line.
    report_traffic(now);

    // RFC section 5.5: a bound player silent for udp_timeout has lost its session. Ended once
    // the loop is over: ending a session erases its peer.
    std::vector<std::uint32_t> silent;
    for (const auto& [token, peer] : peers_) {
        if (now - peer.heard >= udp_timeout) {
            silent.push_back(token);
        }
    }
    for (const std::uint32_t token : silent) {
        time_out(token);
    }

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
    flush_events(now);

    limiter_.prune(now);
    for (const std::string& line : drops_.take_due(now)) {
        core::log_info("UDP: {}", line);
    }
}

// One line per bound player every traffic_report_period: its rates over the last whole
// second, by packet type, the bytes since its endpoint was bound, and its round trip time.
void ServerNetwork::report_traffic(Clock::time_point now) {
    if (!next_traffic_report_) {
        next_traffic_report_ = now + traffic_report_period;
        return;
    }
    if (now < *next_traffic_report_) {
        return;
    }
    next_traffic_report_ = now + traffic_report_period;
    for (const auto& [token, peer] : peers_) {
        const Session* const session = sessions_.find(token);
        if (session == nullptr) {
            continue;
        }
        const net::TrafficCounters& traffic = peer.traffic;
        const net::TrafficCount sent = traffic.last_second(net::Direction::Sent, now);
        const net::TrafficCount received = traffic.last_second(net::Direction::Received, now);
        const auto per_second = [&](net::Direction direction, net::PacketType type) {
            return traffic.last_second(direction, type, now).datagrams;
        };
        core::log_info("UDP: player {} sends {} datagrams/s, {} B/s (snapshots {}, events {}), "
                       "receives {} datagrams/s, {} B/s (inputs {}, events {}, heartbeats {}); "
                       "{} B sent and {} B received in all; {}",
                       session->player, sent.datagrams, sent.bytes,
                       per_second(net::Direction::Sent, net::PacketType::Snapshot),
                       per_second(net::Direction::Sent, net::PacketType::Event), received.datagrams,
                       received.bytes, per_second(net::Direction::Received, net::PacketType::Input),
                       per_second(net::Direction::Received, net::PacketType::Event),
                       per_second(net::Direction::Received, net::PacketType::Heartbeat),
                       traffic.total(net::Direction::Sent).bytes,
                       traffic.total(net::Direction::Received).bytes,
                       describe_rtt(peer.channel.rtt()));
    }
}

const net::TrafficCounters* ServerNetwork::traffic(std::uint32_t token) const {
    const auto peer = peers_.find(token);
    return peer == peers_.end() ? nullptr : &peer->second.traffic;
}

const net::RttEstimator* ServerNetwork::rtt(std::uint32_t token) const {
    const auto peer = peers_.find(token);
    return peer == peers_.end() ? nullptr : &peer->second.channel.rtt();
}

const SnapshotHistory* ServerNetwork::snapshots(std::uint32_t token) const {
    const auto peer = peers_.find(token);
    return peer == peers_.end() ? nullptr : &peer->second.snapshots;
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

std::uint64_t ServerNetwork::dropped_datagrams() const noexcept {
    return dropped_;
}

std::uint64_t ServerNetwork::game_datagrams() const noexcept {
    return game_datagrams_;
}

// The player gets Logout (TimedOut), and its connection closes once it is sent (RFC
// section 3.2); a client that is really gone never reads it.
void ServerNetwork::time_out(std::uint32_t token) {
    if (const Session* const session = sessions_.find(token)) {
        core::log_info("UDP: player {} sent nothing for {} s: session timed out", session->player,
                       udp_timeout.count());
    }
    end_with_logout(token, LogoutReason::TimedOut, LeaveReason::TimedOut);
    ++timeouts_;
}

// RFC section 6.6: the session of a client whose packet the server could not handle ends
// with Logout (ProtocolError); the others play on.
void ServerNetwork::kick(std::uint32_t token) {
    if (const Session* const session = sessions_.find(token)) {
        core::log_warn("player {} ({}) kicked: its packet could not be handled", session->player,
                       session->endpoint ? session->endpoint->to_string() : "not bound");
    }
    end_with_logout(token, LogoutReason::ProtocolError, LeaveReason::Kicked);
}

// A client whose TCP message could not be handled: kicked if logged in, closed otherwise.
void ServerNetwork::kick_client(std::uint64_t id) {
    const auto found = clients_.find(id);
    if (found == clients_.end() || found->second.closed) {
        return;
    }
    if (found->second.token) {
        kick(*found->second.token);
    } else {
        close(found->second, "its message could not be handled");
    }
}

// The player gets Logout with the reason, its connection closes once it is sent (RFC section
// 3.2), and it leaves the game.
void ServerNetwork::end_with_logout(std::uint32_t token, LogoutReason logout, LeaveReason leave) {
    for (auto& [id, client] : clients_) {
        if (client.token == token && !client.closed && !client.close_when_sent) {
            send(client, write_logout(logout));
            client.close_when_sent = true;
        }
    }
    end_session(token, leave);
}

void ServerNetwork::shutdown() {
    listener_.close();
    // Collected first: ending a session changes the clients.
    std::vector<std::uint32_t> players;
    for (auto& [id, client] : clients_) {
        if (client.closed || client.close_when_sent) {
            continue;
        }
        if (client.token) {
            players.push_back(*client.token);
        } else {
            close(client, {});
        }
    }
    for (const std::uint32_t token : players) {
        end_with_logout(token, LogoutReason::Shutdown, LeaveReason::Left);
    }
    core::log_info("TCP: told {} player(s) that the server stops", players.size());
}

bool ServerNetwork::idle() const noexcept {
    return clients_.empty();
}

std::uint64_t ServerNetwork::errors() const noexcept {
    return errors_;
}

std::uint64_t ServerNetwork::timeouts() const noexcept {
    return timeouts_;
}

} // namespace rtype
