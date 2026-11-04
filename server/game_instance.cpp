#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>
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

namespace {

// A component of an entity, or nullptr.
template <typename Component>
const Component* find(const ecs::Registry& world, ecs::Entity entity) {
    const auto component = world.try_get<Component>(entity);
    return component.has_value() ? &component->get() : nullptr;
}

} // namespace

GameInstance::GameInstance(std::optional<std::uint64_t> seed, ecs::WaveSchedule waves)
    : seed_{seed}, waves_{std::move(waves)} {
    start();
}

void GameInstance::start() {
    game_ = std::make_unique<ServerGame>(seed_);
    game_->play_waves(waves_);
    ships_.fill(std::nullopt);
    scores_.fill(0);
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
    scores_.at(player) = 0;
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

bool GameInstance::set_input(std::uint8_t player, ecs::PlayerInput input) {
    const std::optional<ecs::Entity> found = ship(player);
    return found.has_value() && game_->set_input(*found, input);
}

bool GameInstance::set_rewind(std::uint8_t player, std::uint32_t ticks) {
    const std::optional<ecs::Entity> found = ship(player);
    return found.has_value() && game_->set_rewind(*found, ticks);
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

std::optional<PlayerState> GameInstance::player_state(std::uint8_t player) {
    if (player >= ships_.size() || !ships_.at(player).has_value()) {
        return std::nullopt;
    }
    const std::optional<ecs::Entity> alive = ship(player);
    if (!alive) {
        return PlayerState{.ship = std::nullopt, .lives = 0, .score = scores_.at(player)};
    }
    const ecs::Registry& world = game_->registry();
    const auto* const lives = find<ecs::Lives>(world, *alive);
    const auto* const score = find<ecs::Score>(world, *alive);
    if (score != nullptr) {
        scores_.at(player) = score->points;
    }
    return PlayerState{
        .ship = alive, .lives = lives != nullptr ? lives->count : 0, .score = scores_.at(player)};
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
