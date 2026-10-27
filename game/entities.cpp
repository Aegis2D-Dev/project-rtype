#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/movement.hpp>
#include <aegis2d/shmup/components.hpp>
#include <aegis2d/shmup/ships.hpp>

#include <rtype/entities.hpp>
#include <rtype/layers.hpp>
#include <rtype/tuning.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace rtype {

namespace ecs = aegis2d::ecs;
namespace shmup = aegis2d::shmup;
using aegis2d::Vec2;

Vec2 player_spawn(std::size_t slot) {
    if (slot >= tuning::max_players) {
        throw std::out_of_range{"rtype: player slot " + std::to_string(slot) + " is invalid"};
    }
    const float spacing = tuning::area_height / static_cast<float>(tuning::max_players + 1);
    return Vec2{tuning::player_spawn_x, spacing * static_cast<float>(slot + 1)};
}

ecs::Entity spawn_player(ecs::Registry& registry, std::size_t slot) {
    const Vec2 spawn = player_spawn(slot);
    const ecs::Entity ship = shmup::spawn_player_ship(registry, spawn, tuning::player_speed);
    registry.emplace<ecs::Sprite>(ship, ecs::Sprite{.size = tuning::player_size});
    registry.emplace<ecs::Collider>(
        ship,
        ecs::Collider{.size = tuning::player_hitbox, .layer = layer::player, .mask = mask::player});
    registry.emplace<ecs::Health>(ship, tuning::player_health);
    registry.emplace<ecs::Damage>(
        ship, ecs::Damage{.amount = tuning::ram_damage, .consumed_on_hit = false});
    registry.emplace<ecs::Lives>(
        ship,
        ecs::Lives{.count = tuning::player_lives, .spawn = spawn, .health = tuning::player_health});
    registry.emplace<ecs::Score>(ship);
    registry.emplace<shmup::Weapon>(ship, shmup::Weapon{.cooldown = tuning::player_fire_cooldown});
    registry.emplace<ecs::NetworkId>(ship, ecs::NetworkId::of(ship));
    return ship;
}

ecs::Entity spawn_player_missile(ecs::Registry& registry, ecs::Entity shooter) {
    const Vec2 nose{(tuning::player_size.x + tuning::player_missile_size.x) * 0.5F, 0.0F};
    const Vec2 position = registry.get<ecs::Transform>(shooter).position + nose;
    const ecs::Entity missile = registry.create();
    registry.emplace<ecs::Transform>(missile, position);
    registry.emplace<ecs::Velocity>(missile, Vec2{tuning::player_missile_speed, 0.0F});
    registry.emplace<ecs::Sprite>(missile, ecs::Sprite{.size = tuning::player_missile_size});
    registry.emplace<ecs::Collider>(missile, ecs::Collider{.size = tuning::player_missile_size,
                                                           .layer = layer::player_missile,
                                                           .mask = mask::player_missile});
    registry.emplace<ecs::Damage>(missile, tuning::player_missile_damage);
    registry.emplace<ecs::Owner>(missile, shooter);
    registry.emplace<ecs::OffscreenDespawn>(missile, tuning::despawn_margin);
    registry.emplace<ecs::NetworkId>(missile, ecs::NetworkId::of(missile));
    return missile;
}

ecs::Entity spawn_bydos_slave(ecs::Registry& registry, Vec2 line, float phase, float first_shot) {
    const ecs::SineMotion wave{.amplitude = Vec2{0.0F, -tuning::bydos_wave_amplitude},
                               .period = tuning::bydos_wave_period,
                               .elapsed = phase};
    const ecs::Entity bydos = registry.create();
    registry.emplace<ecs::Transform>(bydos, line + ecs::sine_offset(wave, phase));
    registry.emplace<ecs::Velocity>(bydos, Vec2{-tuning::bydos_speed, 0.0F});
    registry.emplace<ecs::SineMotion>(bydos, wave);
    registry.emplace<ecs::Sprite>(bydos, ecs::Sprite{.size = tuning::bydos_size});
    registry.emplace<ecs::Collider>(
        bydos,
        ecs::Collider{.size = tuning::bydos_size, .layer = layer::enemy, .mask = mask::enemy});
    registry.emplace<ecs::Health>(bydos, tuning::bydos_health);
    registry.emplace<ecs::Damage>(
        bydos, ecs::Damage{.amount = tuning::ram_damage, .consumed_on_hit = false});
    registry.emplace<ecs::ScoreValue>(bydos, tuning::bydos_score);
    registry.emplace<shmup::Weapon>(
        bydos, shmup::Weapon{.cooldown = tuning::bydos_fire_cooldown, .reload = first_shot});
    registry.emplace<shmup::AutoTrigger>(bydos);
    registry.emplace<ecs::OffscreenDespawn>(bydos, tuning::despawn_margin);
    registry.emplace<ecs::NetworkId>(bydos, ecs::NetworkId::of(bydos));
    return bydos;
}

ecs::Entity spawn_enemy_missile(ecs::Registry& registry, Vec2 position) {
    const ecs::Entity missile = registry.create();
    registry.emplace<ecs::Transform>(missile, position);
    registry.emplace<ecs::Velocity>(missile, Vec2{-tuning::enemy_missile_speed, 0.0F});
    registry.emplace<ecs::Sprite>(missile, ecs::Sprite{.size = tuning::enemy_missile_size});
    registry.emplace<ecs::Collider>(missile, ecs::Collider{.size = tuning::enemy_missile_size,
                                                           .layer = layer::enemy_missile,
                                                           .mask = mask::enemy_missile});
    registry.emplace<ecs::Damage>(missile, tuning::enemy_missile_damage);
    registry.emplace<ecs::OffscreenDespawn>(missile, tuning::despawn_margin);
    registry.emplace<ecs::NetworkId>(missile, ecs::NetworkId::of(missile));
    return missile;
}

ecs::Entity fire_enemy_missile(ecs::Registry& registry, ecs::Entity shooter) {
    const Vec2 position = registry.get<ecs::Transform>(shooter).position;
    const auto sprite = registry.try_get<ecs::Sprite>(shooter);
    const float half = sprite ? sprite->get().size.x * 0.5F : 0.0F;
    const Vec2 nose{half + (tuning::enemy_missile_size.x * 0.5F), 0.0F};
    const ecs::Entity missile = spawn_enemy_missile(registry, position - nose);
    registry.emplace<ecs::Owner>(missile, shooter);
    return missile;
}

ecs::Entity spawn_enemy(ecs::Registry& registry, std::string_view type, Vec2 position) {
    if (type == "bydos_slave") {
        return spawn_bydos_slave(registry, position);
    }
    throw std::invalid_argument{"rtype: unknown enemy type \"" + std::string{type} + '"'};
}

} // namespace rtype
