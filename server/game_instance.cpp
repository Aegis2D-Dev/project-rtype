#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/waves.hpp>

#include <rtype/game_instance.hpp>
#include <rtype/server_game.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace rtype {

namespace ecs = aegis2d::ecs;

GameInstance::GameInstance(std::optional<std::uint64_t> seed, ecs::WaveSchedule waves)
    : seed_{seed}, waves_{std::move(waves)} {
    start();
}

void GameInstance::start() {
    game_ = std::make_unique<ServerGame>(seed_);
    game_->play_waves(waves_);
    ships_.fill(std::nullopt);
    ++games_;
}

ecs::Entity GameInstance::join(std::uint8_t player) {
    if (player >= ships_.size()) {
        throw std::out_of_range{"rtype: player numbers go from 0 to 3"};
    }
    std::optional<ecs::Entity>& ship = ships_.at(player);
    if (ship) {
        throw std::logic_error{"rtype: this player is in the game already"};
    }
    ship = game_->add_player(player);
    return *ship;
}

bool GameInstance::leave(std::uint8_t player) {
    if (player >= ships_.size()) {
        return false;
    }
    std::optional<ecs::Entity>& ship = ships_.at(player);
    if (!ship.has_value()) {
        return false;
    }
    // A player out of lives has no ship left to remove, but still leaves its number.
    game_->remove_player(*ship);
    ship.reset();
    if (players() == 0) {
        start();
    }
    return true;
}

std::optional<ecs::Entity> GameInstance::ship(std::uint8_t player) const {
    if (player >= ships_.size()) {
        return std::nullopt;
    }
    const std::optional<ecs::Entity>& ship = ships_.at(player);
    if (!ship.has_value() || !game_->slot_of(*ship)) {
        return std::nullopt;
    }
    return ship;
}

std::size_t GameInstance::players() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(
        ships_, [](const std::optional<ecs::Entity>& ship) { return ship.has_value(); }));
}

std::uint64_t GameInstance::games() const noexcept {
    return games_;
}

ServerGame& GameInstance::game() noexcept {
    return *game_;
}

const ServerGame& GameInstance::game() const noexcept {
    return *game_;
}

} // namespace rtype
