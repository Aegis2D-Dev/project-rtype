#include <aegis2d/core/log.hpp>
#include <aegis2d/net/binary.hpp>
#include <aegis2d/net/compression.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/endpoint.hpp>
#include <aegis2d/net/packet.hpp>
#include <aegis2d/net/reliability.hpp>
#include <aegis2d/net/rtt.hpp>
#include <aegis2d/net/socket_error.hpp>
#include <aegis2d/net/tcp_connection.hpp>
#include <aegis2d/net/traffic.hpp>
#include <aegis2d/net/udp_socket.hpp>

#include <rtype/client_session.hpp>
#include <rtype/client_world.hpp>
#include <rtype/delta_snapshot.hpp>
#include <rtype/events.hpp>
#include <rtype/input.hpp>
#include <rtype/login.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/snapshot_decoder.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;
namespace core = aegis2d::core;

namespace {

// RFC section 5.5: a peer that leaves sends Disconnect three times, as it is not resent.
constexpr int disconnect_copies = 3;

// What the player is told for each LoginRejected reason; unknown ones are a plain refusal.
std::string rejection_text(std::uint8_t reason) {
    switch (static_cast<LoginRejection>(reason)) {
    case LoginRejection::ServerFull:
        return "the server is full";
    case LoginRejection::BadVersion:
        return "the server speaks another version of the protocol";
    case LoginRejection::BadName:
        return "the name is not valid: 1 to 16 characters, no control character";
    case LoginRejection::NameTaken:
        return "another player has this name";
    case LoginRejection::GameStarted:
        return "the game has started";
    }
    return "the server refused the login";
}

std::string logout_text(std::uint8_t reason) {
    switch (static_cast<LogoutReason>(reason)) {
    case LogoutReason::Shutdown:
        return "the server stopped";
    case LogoutReason::TimedOut:
        return "the server lost the connection (timed out)";
    case LogoutReason::ProtocolError:
        return "the server ended the session after an invalid packet";
    case LogoutReason::Left:
        break;
    }
    return "the server stopped";
}

// The bytes a reader has not read yet: the payload after the header.
std::span<const std::byte> datagram_payload(net::BinaryReader& reader) {
    const std::optional<std::span<const std::byte>> rest = reader.read_bytes(reader.remaining());
    return rest.value_or(std::span<const std::byte>{});
}

std::optional<net::Endpoint> resolve(std::string_view host, std::uint16_t port) {
    if (host == "localhost") {
        return net::Endpoint::loopback(port);
    }
    return net::Endpoint::parse(host, port);
}

} // namespace

std::string_view to_string(ClientState state) noexcept {
    switch (state) {
    case ClientState::Idle:
        return "idle";
    case ClientState::Connecting:
        return "connecting";
    case ClientState::LoggingIn:
        return "logging in";
    case ClientState::Binding:
        return "binding";
    case ClientState::InGame:
        return "in game";
    case ClientState::Failed:
        return "failed";
    case ClientState::Lost:
        return "lost";
    case ClientState::Closed:
        return "closed";
    }
    return "unknown";
}

ClientSession::ClientSession(net::NetworkContext& context, ClientSettings settings)
    : context_{context}, settings_{std::move(settings)} {}

ClientSession::~ClientSession() = default;

void ClientSession::start() {
    if (state_ != ClientState::Idle) {
        return;
    }
    started_ = Clock::now();
    const std::optional<net::Endpoint> server = resolve(settings_.host, settings_.tcp_port);
    if (!server.has_value()) {
        fail(std::format("\"{}\" is not an IP address", settings_.host));
        return;
    }
    if (!valid_player_name(settings_.name)) {
        fail(rejection_text(static_cast<std::uint8_t>(LoginRejection::BadName)));
        return;
    }
    const net::Endpoint target = server.value_or(net::Endpoint{});
    server_tcp_ = target;
    state_ = ClientState::Connecting;
    connection_ = std::make_unique<net::TcpConnection>(context_);
    connection_->connect(target, [this, target](const std::error_code& error) {
        if (state_ != ClientState::Connecting) {
            return;
        }
        if (error) {
            fail(std::format("cannot reach {}: {}", target.to_string(), error.message()));
            return;
        }
        on_connected();
    });
}

void ClientSession::on_connected() {
    state_ = ClientState::LoggingIn;
    connection_->start([this](std::span<const std::byte> message) { on_message(message); },
                       [this](net::TcpCloseReason /*reason*/) {
                           if (state_ != ClientState::Failed && state_ != ClientState::Lost &&
                               state_ != ClientState::Closed) {
                               fail("the server closed the connection");
                           }
                       });
    connection_->send(
        write_login_request(LoginRequest{.version = protocol_version, .name = settings_.name}));
    ++sent_;
}

void ClientSession::on_message(std::span<const std::byte> message) {
    if (state_ == ClientState::LoggingIn) {
        if (const std::optional<std::uint8_t> reason = read_login_rejected(message)) {
            fail(rejection_text(*reason));
            return;
        }
        const std::optional<LoginAccepted> accepted = read_login_accepted(message);
        if (!accepted.has_value()) {
            fail("the server sent an unexpected answer to the login");
            return;
        }
        const LoginAccepted granted = accepted.value_or(LoginAccepted{});
        const std::optional<net::Endpoint> udp =
            resolve(server_tcp_.value_or(net::Endpoint{}).address(), granted.udp_port);
        if (!udp.has_value()) {
            fail("the server's address cannot be used for UDP");
            return;
        }
        const net::Endpoint server = udp.value_or(net::Endpoint{});
        login_ = granted;
        server_udp_ = server;
        // RFC section 3.4: from an endpoint of the same family as the server's.
        try {
            socket_ = server.is_v6()
                          ? std::make_unique<net::UdpSocket>(
                                context_, net::Endpoint::v6(std::array<std::uint8_t, 16>{}, 0))
                          : std::make_unique<net::UdpSocket>(context_, std::uint16_t{0});
        } catch (const net::SocketError& error) {
            fail(std::format("cannot open a UDP socket: {}", error.what()));
            return;
        }
        socket_->start_receiving(
            [this](const net::Endpoint& from, std::span<const std::byte> datagram) {
                on_datagram(from, datagram);
            });
        state_ = ClientState::Binding;
        accepted_ = Clock::now();
        core::log_info("client: logged in as player {}, binding UDP port {}", granted.player,
                       granted.udp_port);
        send_connect(accepted_);
        return;
    }
    // Once logged in, the server only sends Logout (RFC section 3.3).
    if (const std::optional<std::uint8_t> reason = read_logout(message)) {
        fail(logout_text(*reason));
        return;
    }
    fail("the server sent an unexpected message");
}

void ClientSession::on_datagram(const net::Endpoint& from, std::span<const std::byte> datagram) {
    // RFC section 3.4: only the server's udp_port, with the session's token.
    if (!server_udp_.has_value() || !login_.has_value() ||
        from != server_udp_.value_or(net::Endpoint{})) {
        return;
    }
    const std::uint32_t token = login_.value_or(LoginAccepted{}).session;
    net::BinaryReader reader{datagram};
    const std::variant<net::PacketHeader, net::HeaderError> read = net::read_header(reader);
    const auto* const header = std::get_if<net::PacketHeader>(&read);
    if (header == nullptr || header->session != token) {
        return;
    }
    const auto now = Clock::now();
    traffic_.record(net::Direction::Received, header->type, datagram.size(), now);
    network_stats_.record(header->sequence, header->type, now);
    // RFC section 2.6: a compressed payload is decompressed before anything reads it.
    std::vector<std::byte> decompressed;
    if (header->compressed) {
        std::optional<std::vector<std::byte>> payload = net::read_payload(*header, reader);
        if (!payload) {
            return;
        }
        decompressed = std::move(payload).value_or(std::vector<std::byte>{});
        reader = net::BinaryReader{decompressed};
    }
    if (state_ == ClientState::Binding) {
        if (header->type != net::PacketType::Accept || !reader.at_end()) {
            return;
        }
        channel_.receive(*header, now);
        state_ = ClientState::InGame;
        next_input_ = now;
        last_heard_ = now;
        world_.join(login_.value_or(LoginAccepted{}).player, settings_.name);
        core::log_info("client: in game as player {}", login_.value_or(LoginAccepted{}).player);
        return;
    }
    if (state_ == ClientState::InGame) {
        on_game_datagram(*header, reader, now);
    }
}

// RFC sections 4.3, 4.4, 5.1 and 6.3: a payload is read whole before anything is applied
// or acknowledged; a duplicate or late datagram is acknowledged but its snapshot ignored, and
// a delta snapshot whose baseline the client no longer has is neither.
void ClientSession::on_game_datagram(const net::PacketHeader& header, net::BinaryReader& payload,
                                     Clock::time_point now) {
    switch (header.type) {
    case net::PacketType::Snapshot: {
        const std::optional<DeltaSnapshot> part = read_delta_snapshot(datagram_payload(payload));
        if (!part.has_value()) {
            return;
        }
        last_heard_ = now;
        // A delta whose baseline is gone is not acknowledged: the server must not build on
        // a snapshot the client could not rebuild.
        const DeltaSnapshot& delta = part.value_or(DeltaSnapshot{});
        if (!snapshots_.decodable(delta)) {
            static_cast<void>(snapshots_.decode(delta)); // Counted in dropped().
            return;
        }
        if (channel_.receive(header, now)) {
            if (const std::optional<Snapshot> snapshot = snapshots_.decode(delta)) {
                world_.apply(snapshot.value_or(Snapshot{}), now);
            }
        }
        return;
    }
    case net::PacketType::Event: {
        // Each event is handed over once, however many copies arrive (section 5.3); one
        // whose bytes do not match its type is ignored on its own (section 6.4).
        std::vector<std::vector<std::byte>> events;
        if (!channel_.read_events(header, payload, events, now)) {
            return;
        }
        last_heard_ = now;
        for (const std::vector<std::byte>& bytes : events) {
            if (const std::optional<GameEvent> event = read_event(bytes)) {
                world_.apply(event.value_or(GameEvent{}), now);
            }
        }
        return;
    }
    case net::PacketType::Heartbeat:
    case net::PacketType::Accept:
        if (payload.at_end()) {
            last_heard_ = now;
            channel_.receive(header, now);
        }
        return;
    case net::PacketType::Disconnect:
        // RFC section 5.5: the server leaves; the first copy ends the session.
        if (payload.at_end()) {
            fail("the server ended the session");
        }
        return;
    case net::PacketType::Connect:
    case net::PacketType::Input:
        return;
    }
}

void ClientSession::send_connect(Clock::time_point now) {
    if (!socket_ || !server_udp_.has_value() || !login_.has_value()) {
        return;
    }
    net::BinaryWriter writer;
    net::write_header(writer, channel_.next_header(net::PacketType::Connect,
                                                   login_.value_or(LoginAccepted{}).session, now));
    socket_->send_to(server_udp_.value_or(net::Endpoint{}), writer.data());
    traffic_.record(net::Direction::Sent, net::PacketType::Connect, writer.data().size(), now);
    last_connect_ = now;
    ++connects_sent_;
}

void ClientSession::update(Clock::time_point now) {
    if (closing_ && connection_ && connection_->stats().sent >= sent_) {
        connection_->close();
        closing_ = false;
    }
    switch (state_) {
    case ClientState::Connecting:
    case ClientState::LoggingIn:
        if (now - started_ >= login_timeout) {
            fail("the server does not answer");
        }
        break;
    case ClientState::Binding:
        if (now - accepted_ >= bind_timeout) {
            // RFC section 3.4: give up, say so over TCP, and blame the UDP port.
            leave();
            state_ = ClientState::Failed;
            error_ = std::format("cannot reach the server over UDP (port {}): a firewall may "
                                 "block it",
                                 login_.value_or(LoginAccepted{}).udp_port);
            core::log_info("client: {}", error_);
        } else if (now - last_connect_ >= connect_interval) {
            send_connect(now);
        }
        break;
    case ClientState::InGame:
        // RFC section 5.5: silent for server_timeout, the server is gone. It is told over TCP,
        // in case only its UDP traffic is lost.
        if (now - last_heard_ >= server_timeout) {
            leave();
            state_ = ClientState::Lost;
            error_ = std::format("connection lost: the server sent nothing for {} seconds",
                                 server_timeout.count());
            core::log_info("client: {}", error_);
            break;
        }
        // One Input per tick: the client is never silent for the 50 ms of section 5.5, so
        // it needs no Heartbeat in game.
        if (now >= next_input_) {
            send_input(now);
        }
        world_.prune(now);
        break;
    case ClientState::Idle:
    case ClientState::Failed:
    case ClientState::Lost:
    case ClientState::Closed:
        break;
    }
}

void ClientSession::set_input(aegis2d::ecs::PlayerInput input) noexcept {
    held_ = static_cast<std::uint8_t>(input.held & valid_actions);
}

// RFC section 4.2: the newest input's own sequence, 0 for the first, then one byte of held
// actions for it and each of the 3 before it; the header is numbered and acknowledges like
// any datagram's.
void ClientSession::send_input(Clock::time_point now) {
    if (!socket_ || !server_udp_.has_value() || !login_.has_value()) {
        return;
    }
    net::BinaryWriter writer;
    net::write_header(writer, channel_.next_header(net::PacketType::Input,
                                                   login_.value_or(LoginAccepted{}).session, now));
    // The 3 inputs before it go along, so a lost Input arrives with one of the next three.
    if (recent_inputs_.size() == max_inputs_per_datagram) {
        recent_inputs_.erase(recent_inputs_.begin());
    }
    recent_inputs_.push_back(
        InputRecord{.sequence = static_cast<std::uint16_t>(inputs_sent_), .actions = held_});
    writer.write_bytes(write_inputs(recent_inputs_));
    socket_->send_to(server_udp_.value_or(net::Endpoint{}), writer.data());
    traffic_.record(net::Direction::Sent, net::PacketType::Input, writer.data().size(), now);
    ++inputs_sent_;
    // On time: the next one an interval after this one's due time. Late by more than an
    // interval (a stall): an interval from now, rather than a burst to catch up.
    next_input_ += std::chrono::duration_cast<Clock::duration>(input_interval);
    if (next_input_ <= now) {
        next_input_ = now + std::chrono::duration_cast<Clock::duration>(input_interval);
    }
}

void ClientSession::leave() {
    if (state_ == ClientState::InGame && socket_ && server_udp_.has_value() && login_.has_value()) {
        const std::uint32_t token = login_.value_or(LoginAccepted{}).session;
        const net::Endpoint server = server_udp_.value_or(net::Endpoint{});
        for (int copy = 0; copy < disconnect_copies; ++copy) {
            net::BinaryWriter writer;
            net::write_header(writer, channel_.next_header(net::PacketType::Disconnect, token));
            socket_->send_to(server, writer.data());
            traffic_.record(net::Direction::Sent, net::PacketType::Disconnect, writer.data().size(),
                            Clock::now());
        }
    }
    if (connection_ && connection_->is_open() &&
        (state_ == ClientState::Binding || state_ == ClientState::InGame)) {
        connection_->send(write_logout(LogoutReason::Left));
        ++sent_;
        closing_ = true;
    } else if (connection_) {
        connection_->close();
    }
    if (socket_) {
        socket_->close();
    }
    state_ = ClientState::Closed;
}

void ClientSession::fail(std::string reason) {
    // A session that was running is lost; one that never started has failed.
    state_ = state_ == ClientState::InGame ? ClientState::Lost : ClientState::Failed;
    error_ = std::move(reason);
    core::log_info("client: {}", error_);
    if (connection_ && !closing_) {
        connection_->close();
    }
    if (socket_) {
        socket_->close();
    }
}

ClientState ClientSession::state() const noexcept {
    return state_;
}

const std::string& ClientSession::error() const noexcept {
    return error_;
}

const std::optional<LoginAccepted>& ClientSession::login() const noexcept {
    return login_;
}

const std::optional<net::Endpoint>& ClientSession::server_udp() const noexcept {
    return server_udp_;
}

std::uint64_t ClientSession::connects_sent() const noexcept {
    return connects_sent_;
}

std::uint64_t ClientSession::inputs_sent() const noexcept {
    return inputs_sent_;
}

std::optional<std::uint16_t> ClientSession::last_input() const noexcept {
    if (inputs_sent_ == 0) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(inputs_sent_ - 1);
}

std::optional<std::uint16_t> ClientSession::acknowledged_input() const noexcept {
    if (!login_.has_value()) {
        return std::nullopt;
    }
    const std::uint8_t player = login_.value_or(LoginAccepted{}).player;
    if (player >= world_.players().size()) {
        return std::nullopt;
    }
    const std::optional<RemotePlayer>& own = world_.players().at(player);
    return own.has_value() ? own->input : std::nullopt;
}

const net::TrafficCounters& ClientSession::traffic() const noexcept {
    return traffic_;
}

const net::RttEstimator& ClientSession::rtt() const noexcept {
    return channel_.rtt();
}

const NetworkStats& ClientSession::network_stats() const noexcept {
    return network_stats_;
}

ClientWorld& ClientSession::world() noexcept {
    return world_;
}

const ClientWorld& ClientSession::world() const noexcept {
    return world_;
}

const SnapshotDecoder& ClientSession::snapshots() const noexcept {
    return snapshots_;
}

} // namespace rtype
