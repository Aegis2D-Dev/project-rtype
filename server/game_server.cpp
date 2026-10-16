#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/context.hpp>

#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/game_server.hpp>
#include <rtype/server_network.hpp>
#include <rtype/sessions.hpp>
#include <rtype/simulation.hpp>
#include <rtype/snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace rtype {

GameServer::GameServer(aegis2d::net::NetworkContext& context, GameInstance& instance,
                       std::uint16_t tcp_port, std::uint16_t udp_port, std::uint32_t rate)
    : context_{context}, network_{context, tcp_port, udp_port},
      simulation_{instance, rate, [this](GameInstance& game) { after_tick(game); }} {
    network_.on_players(
        [this](const Session& session) { on_joined(session); },
        [this](const Session& session) {
            simulation_.post([player = session.player](GameInstance& game) { game.leave(player); });
        });
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
    context_.post([this, payloads = write_snapshot(take_snapshot(instance))] {
        network_.send_snapshot(payloads);
    });
}

// The new player starts from the world as it is (RFC section 4.6); its own ship comes with
// the next tick's events, like for the others.
void GameServer::on_joined(const Session& session) {
    simulation_.post([this, player = session.player, token = session.token](GameInstance& game) {
        game.join(player);
        if (std::vector<std::vector<std::byte>> world = write_events(events_.spawns(game));
            !world.empty()) {
            context_.post(
                [this, token, world = std::move(world)] { network_.send_events(world, token); });
        }
    });
}

ServerNetwork& GameServer::network() noexcept {
    return network_;
}

Simulation& GameServer::simulation() noexcept {
    return simulation_;
}

} // namespace rtype
