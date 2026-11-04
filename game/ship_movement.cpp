#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/movement.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/ecs/system.hpp>
#include <aegis2d/shmup/components.hpp>

#include <rtype/server_game.hpp>
#include <rtype/ship_movement.hpp>
#include <rtype/tuning.hpp>

namespace rtype {

namespace ecs = aegis2d::ecs;
namespace shmup = aegis2d::shmup;
using aegis2d::Vec2;

Vec2 ship_velocity(ecs::PlayerInput input) noexcept {
    return ecs::direction_of(input) * tuning::player_speed;
}

Vec2 keep_ship_in_area(Vec2 position) noexcept {
    return ecs::clamp_into(position, tuning::player_size * 0.5F, ServerGame::play_area());
}

ShipMotion step_ship(Vec2 position, ecs::PlayerInput input, float delta) noexcept {
    const Vec2 velocity = ship_velocity(input);
    // As ecs::apply_velocity() moves it, then as keep_ships_in_area() clamps it.
    position += velocity * delta;
    return ShipMotion{.position = keep_ship_in_area(position), .velocity = velocity};
}

void steer_ships(ecs::Registry& registry, const ecs::Tick& /*tick*/) {
    registry.view<shmup::PlayerShip, ecs::PlayerInput, ecs::Velocity>().each(
        [](const shmup::PlayerShip& /*ship*/, const ecs::PlayerInput& input,
           ecs::Velocity& velocity) { velocity.value = ship_velocity(input); });
}

void keep_ships_in_area(ecs::Registry& registry, const ecs::Tick& /*tick*/) {
    registry.view<shmup::PlayerShip, ecs::Transform>().each(
        [](const shmup::PlayerShip& /*ship*/, ecs::Transform& transform) {
            transform.position = keep_ship_in_area(transform.position);
        });
}

} // namespace rtype
