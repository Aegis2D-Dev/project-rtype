#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/context.hpp>

#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/game_server.hpp>
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

GameServer::GameServer(aegis2d::net::NetworkContext& context, GameInstance& instance,
                       std::uint16_t tcp_port, std::uint16_t udp_port, std::uint32_t rate)
    : context_{context}, network_{context, tcp_port, udp_port},
      simulation_{instance, rate, [this](GameInstance& game) { after_tick(game); }} {
    network_.on_players(
        [this](const Session& session) { on_joined(session); },
        [this](const Session& session, LeaveReason reason) { on_left(session, reason); });
    network_.on_input([this](const Session& session, aegis2d::ecs::PlayerInput input) {
        simulation_.post([player = session.player, input](GameInstance& game) {
            game.set_input(player, input);
        });
    });
}

// On the simulation thread: what changed goes to the network thread as events; every third
// tick, the state of the game goes too, as a snapshot.
void GameServer::after_tick(GameInstance& instance) {
    if (std::vector<std::vector<std::byte>> changes = write_events(events_.update(instance));
        !changes.empty()) {
        context_.post([this, changes = std::move(changes)] { network_.send_events(changes); });
    }
    if (!snapshot_due(instance.game().ticks())) {
        return;
    }
    // Kept by each player's history, as a possible baseline of the next ones.
    auto snapshot = std::make_shared<const Snapshot>(take_snapshot(instance));
    std::vector<std::vector<std::byte>> payloads = write_snapshot(*snapshot);
    context_.post([this, payloads = std::move(payloads), snapshot = std::move(snapshot)] {
        network_.send_snapshot(payloads, snapshot);
    });
}

// RFC section 4.6: the others learn who joined; the new player learns who is there, then
// gets the world as it is. Its own ship comes with the next tick's events, like for the
// others.
void GameServer::on_joined(const Session& session) {
    simulation_.post([this, player = session.player, token = session.token,
                      name = session.name](GameInstance& game) {
        game.join(player);
        const auto tick = static_cast<std::uint32_t>(game.game().ticks());
        std::vector<GameEvent> welcome;
        for (std::uint8_t other = 0; other < tuning::max_players; ++other) {
            if (const std::optional<std::string>& other_name = names_.at(other); other_name) {
                welcome.emplace_back(
                    PlayerJoinedEvent{.tick = tick, .player = other, .name = *other_name});
            }
        }
        for (GameEvent& spawn : events_.spawns(game)) {
            welcome.push_back(std::move(spawn));
        }
        names_.at(player) = name;
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
    simulation_.post([this, player = session.player, reason](GameInstance& game) {
        names_.at(player).reset();
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

Simulation& GameServer::simulation() noexcept {
    return simulation_;
}

} // namespace rtype
