#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/context.hpp>

#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/game_server.hpp>
#include <rtype/room.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>
#include <rtype/simulation.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>
#include <rtype/world_snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rtype {

GameServer::GameServer(aegis2d::net::NetworkContext& context, std::optional<std::uint64_t> seed,
                       aegis2d::ecs::WaveSchedule waves, std::uint16_t tcp_port,
                       std::uint16_t udp_port, std::uint32_t rate)
    : context_{context}, network_{context, tcp_port, udp_port},
      room_{seed, std::move(waves), rate,
            [this](Room& room, GameInstance& game) { after_tick(room, game); }} {
    network_.on_players(
        [this](const Session& session) { on_joined(session); },
        [this](const Session& session, LeaveReason reason) { on_left(session, reason); });
    network_.on_input([this](const Session& session, aegis2d::ecs::PlayerInput input) {
        room_.simulation().post([player = session.player, input](GameInstance& game) {
            game.set_input(player, input);
        });
    });
}

// On the simulation thread: what changed goes to the network thread as events; every third
// tick, the state of the game goes too, as a snapshot.
void GameServer::after_tick(Room& room, GameInstance& instance) {
    if (std::vector<std::vector<std::byte>> changes = write_events(room.events().update(instance));
        !changes.empty()) {
        context_.post([this, changes = std::move(changes)] { network_.send_events(changes); });
    }
    if (!snapshot_due(instance.game().ticks())) {
        return;
    }
    // Encoded for each player against its own baseline, and kept by its history as a
    // possible baseline of the next ones.
    auto snapshot = std::make_shared<const Snapshot>(take_snapshot(instance));
    context_.post([this, snapshot = std::move(snapshot)] { network_.send_snapshot(snapshot); });
}

// RFC section 4.6: the others learn who joined; the new player learns who is there, then
// gets the world as it is. Its own ship comes with the next tick's events, like for the
// others.
void GameServer::on_joined(const Session& session) {
    room_.simulation().post([this, player = session.player, token = session.token,
                             name = session.name](GameInstance& game) {
        game.join(player);
        const auto tick = static_cast<std::uint32_t>(game.game().ticks());
        std::vector<GameEvent> welcome;
        for (std::uint8_t other = 0; other < tuning::max_players; ++other) {
            if (const std::optional<std::string>& other_name = room_.names().at(other);
                other_name) {
                welcome.emplace_back(
                    PlayerJoinedEvent{.tick = tick, .player = other, .name = *other_name});
            }
        }
        for (GameEvent& spawn : room_.events().spawns(game)) {
            welcome.push_back(std::move(spawn));
        }
        room_.names().at(player) = name;
        std::vector<std::vector<std::byte>> joined = write_events(std::vector<GameEvent>{
            PlayerJoinedEvent{.tick = tick, .player = player, .name = name}});
        context_.post([this, token, joined = std::move(joined), welcome = write_events(welcome)] {
            network_.send_events_to_others(joined, token);
            if (!welcome.empty()) {
                network_.send_events(welcome, token);
            }
        });
    });
}

// RFC section 4.6: the others learn that the player left and why, before the Destroy of its
// ship, which comes with the next tick's events.
void GameServer::on_left(const Session& session, LeaveReason reason) {
    room_.simulation().post([this, player = session.player, reason](GameInstance& game) {
        room_.names().at(player).reset();
        if (!game.leave(player)) {
            return;
        }
        const auto tick = static_cast<std::uint32_t>(game.game().ticks());
        context_.post([this, left = write_events(std::vector<GameEvent>{PlayerLeftEvent{
                                 .tick = tick, .player = player, .reason = reason}})] {
            network_.send_events(left);
        });
    });
}

ServerNetwork& GameServer::network() noexcept {
    return network_;
}

Room& GameServer::room() noexcept {
    return room_;
}

Simulation& GameServer::simulation() noexcept {
    return room_.simulation();
}

} // namespace rtype
