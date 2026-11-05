#include <aegis2d/core/log.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/context.hpp>
#include <aegis2d/net/rtt.hpp>

#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/game_server.hpp>
#include <rtype/lag_compensation.hpp>
#include <rtype/room.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>
#include <rtype/simulation.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>
#include <rtype/world_snapshot.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rtype {

namespace {

// Sessions spread over the rooms of the server.
SessionTable sessions_in(std::uint32_t rooms) {
    SessionTable sessions;
    sessions.set_rooms(rooms);
    return sessions;
}

} // namespace

GameServer::GameServer(aegis2d::net::NetworkContext& context, std::optional<std::uint64_t> seed,
                       aegis2d::ecs::WaveSchedule waves, std::uint16_t tcp_port,
                       std::uint16_t udp_port, std::uint32_t rate, std::uint32_t max_rooms)
    : context_{context}, network_{context, tcp_port, udp_port, sessions_in(max_rooms)}, seed_{seed},
      waves_{std::move(waves)}, rate_{rate} {
    rooms_.resize(network_.sessions().rooms());
    network_.on_players(
        [this](const Session& session) { on_joined(session); },
        [this](const Session& session, LeaveReason reason) { on_left(session, reason); });
    network_.on_input(
        [this](const Session& session, aegis2d::ecs::PlayerInput input, std::uint16_t sequence) {
            room(session.room)
                .simulation()
                .post([player = session.player, input, sequence](GameInstance& game) {
                    game.set_input(player, input, sequence);
                });
        });
}

// On the simulation thread of a room: what changed goes to the network thread as events for
// the players of the room; every third tick, the state of its game goes too, as a snapshot.
void GameServer::after_tick(Room& room, GameInstance& instance) {
    const std::uint32_t id = room.id();
    if (std::vector<std::vector<std::byte>> changes = write_events(room.events().update(instance));
        !changes.empty()) {
        context_.post(
            [this, id, changes = std::move(changes)] { network_.send_events(changes, id); });
    }
    if (!snapshot_due(instance.game().ticks())) {
        return;
    }
    // Encoded for each player against its own baseline, and kept by its history as a
    // possible baseline of the next ones.
    auto snapshot = std::make_shared<const Snapshot>(take_snapshot(instance));
    context_.post([this, id, snapshot = std::move(snapshot)] {
        network_.send_snapshot(snapshot, id);
        update_rewinds(id);
    });
}

// On the network thread: the smoothed round trip time of each player of the room, as the
// ticks its missiles are compensated by, goes to its game when it changes.
void GameServer::update_rewinds(std::uint32_t id) {
    for (auto& [token, seat] : seats_) {
        if (seat.room != id) {
            continue;
        }
        const aegis2d::net::RttEstimator* const rtt = network_.rtt(token);
        if (rtt == nullptr || !rtt->has_estimate()) {
            continue;
        }
        const std::uint32_t ticks =
            rewind_ticks(rtt->smoothed(), tuning::client_interpolation_delay);
        if (ticks == seat.rewind) {
            continue;
        }
        seat.rewind = ticks;
        room(id).simulation().post(
            [player = seat.player, ticks](GameInstance& game) { game.set_rewind(player, ticks); });
    }
}

// RFC section 4.6: the others of the room learn who joined; the new player learns who is
// there, then gets the world as it is. Its own ship comes with the next tick's events, like
// for the others.
void GameServer::on_joined(const Session& session) {
    seats_.insert_or_assign(session.token,
                            Seat{.room = session.room, .player = session.player, .rewind = 0});
    Room& joined_room = open_room(session.room);
    joined_room.simulation().post([this, &joined_room, player = session.player,
                                   token = session.token, name = session.name](GameInstance& game) {
        game.join(player);
        const auto tick = static_cast<std::uint32_t>(game.game().ticks());
        std::vector<GameEvent> welcome;
        for (std::uint8_t other = 0; other < tuning::max_players; ++other) {
            if (const std::optional<std::string>& other_name = joined_room.names().at(other);
                other_name) {
                welcome.emplace_back(
                    PlayerJoinedEvent{.tick = tick, .player = other, .name = *other_name});
            }
        }
        for (GameEvent& spawn : joined_room.events().spawns(game)) {
            welcome.push_back(std::move(spawn));
        }
        joined_room.names().at(player) = name;
        std::vector<std::vector<std::byte>> joined = write_events(std::vector<GameEvent>{
            PlayerJoinedEvent{.tick = tick, .player = player, .name = name}});
        context_.post([this, id = joined_room.id(), token, joined = std::move(joined),
                       welcome = write_events(welcome)] {
            network_.send_events_to_others(joined, id, token);
            if (!welcome.empty()) {
                network_.send_events(welcome, id, token);
            }
        });
    });
}

// RFC section 4.6: the others of the room learn that the player left and why, before the
// Destroy of its ship, which comes with the next tick's events. The last one closes it.
void GameServer::on_left(const Session& session, LeaveReason reason) {
    seats_.erase(session.token);
    if (std::ranges::none_of(seats_,
                             [&](const auto& seat) { return seat.second.room == session.room; })) {
        close_room(session.room);
        return;
    }
    Room& left_room = room(session.room);
    left_room.simulation().post([this, &left_room, player = session.player,
                                 reason](GameInstance& game) {
        left_room.names().at(player).reset();
        if (!game.leave(player)) {
            return;
        }
        const auto tick = static_cast<std::uint32_t>(game.game().ticks());
        context_.post([this, id = left_room.id(),
                       left = write_events(std::vector<GameEvent>{
                           PlayerLeftEvent{.tick = tick, .player = player, .reason = reason}})] {
            network_.send_events(left, id);
        });
    });
}

// The room of a player who joins, opened with its game on a new thread if it was not.
Room& GameServer::open_room(std::uint32_t id) {
    std::unique_ptr<Room>& slot = rooms_.at(id);
    if (!slot) {
        slot = std::make_unique<Room>(
            id, seed_, waves_, rate_,
            [this](Room& room, GameInstance& game) { after_tick(room, game); });
        aegis2d::core::log_info("server: room {} opened ({} of {} open)", id, open_rooms(),
                                max_rooms());
    }
    return *slot;
}

// Stops the thread of the room and waits for it, then forgets its game. What the thread
// posted for the network thread before it stopped only goes to the sessions of that room,
// none now: a player who joins it next gets a new game.
void GameServer::close_room(std::uint32_t id) {
    const std::unique_ptr<Room> closed = std::move(rooms_.at(id));
    if (!closed) {
        return;
    }
    closed->simulation().stop();
    aegis2d::core::log_info("server: room {} closed after {} ticks ({} of {} open)", id,
                            closed->simulation().ticks(), open_rooms(), max_rooms());
}

ServerNetwork& GameServer::network() noexcept {
    return network_;
}

Room& GameServer::room(std::uint32_t id) {
    const std::unique_ptr<Room>& found = rooms_.at(id);
    if (!found) {
        throw std::out_of_range{std::format("room {} is not open", id)};
    }
    return *found;
}

std::uint32_t GameServer::open_rooms() const noexcept {
    return static_cast<std::uint32_t>(
        std::ranges::count_if(rooms_, [](const auto& room) { return room != nullptr; }));
}

std::uint32_t GameServer::max_rooms() const noexcept {
    return static_cast<std::uint32_t>(rooms_.size());
}

std::uint32_t GameServer::rate() const noexcept {
    return rate_;
}

Simulation& GameServer::simulation() {
    return room().simulation();
}

} // namespace rtype
