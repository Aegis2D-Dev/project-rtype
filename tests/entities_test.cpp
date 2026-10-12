#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/collision.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/entities.hpp>
#include <rtype/layers.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <vector>

using aegis2d::Vec2;
using aegis2d::ecs::Collider;
using aegis2d::ecs::CollisionGrid;
using aegis2d::ecs::Contact;
using aegis2d::ecs::Damage;
using aegis2d::ecs::Entity;
using aegis2d::ecs::find_contacts;
using aegis2d::ecs::Health;
using aegis2d::ecs::hits;
using aegis2d::ecs::Lives;
using aegis2d::ecs::NetworkId;
using aegis2d::ecs::Owner;
using aegis2d::ecs::Registry;
using aegis2d::ecs::Score;
using aegis2d::ecs::ScoreValue;
using aegis2d::ecs::Transform;
using aegis2d::ecs::Velocity;
namespace layer = rtype::layer;
namespace mask = rtype::mask;
namespace tuning = rtype::tuning;

namespace {

std::vector<Contact> contacts_of(Registry& registry) {
    CollisionGrid grid{Vec2{tuning::area_width, tuning::area_height}, tuning::collision_cell};
    grid.build(registry);
    std::vector<Contact> contacts = find_contacts(registry, grid);
    std::ranges::sort(contacts);
    return contacts;
}

bool touching(const std::vector<Contact>& contacts, Entity a, Entity b) {
    return std::ranges::any_of(contacts, [a, b](const Contact& contact) {
        return (contact.first == a && contact.second == b) ||
               (contact.first == b && contact.second == a);
    });
}

} // namespace

TEST_CASE("the four player slots start apart on the left", "[rtype][entities]") {
    std::set<float> heights;
    for (std::size_t slot = 0; slot < tuning::max_players; ++slot) {
        const Vec2 spawn = rtype::player_spawn(slot);
        CHECK(spawn.x == tuning::player_spawn_x);
        CHECK(spawn.y > 0.0F);
        CHECK(spawn.y < tuning::area_height);
        heights.insert(spawn.y);
    }
    CHECK(heights.size() == tuning::max_players);
    CHECK_THROWS_AS(rtype::player_spawn(tuning::max_players), std::out_of_range);
}

TEST_CASE("a player ship has everything the server needs", "[rtype][entities]") {
    Registry registry;
    const Entity ship = rtype::spawn_player(registry, 2);
    CHECK(registry.get<Transform>(ship).position == rtype::player_spawn(2));
    CHECK(registry.get<Collider>(ship).layer == layer::player);
    CHECK(registry.get<Collider>(ship).mask == mask::player);
    CHECK(registry.get<Health>(ship).points == tuning::player_health);
    CHECK(registry.get<Lives>(ship).count == tuning::player_lives);
    CHECK(registry.get<Lives>(ship).spawn == rtype::player_spawn(2));
    CHECK(registry.get<Score>(ship).points == 0);
    CHECK_FALSE(registry.get<Damage>(ship).consumed_on_hit);
    CHECK(registry.get<NetworkId>(ship) == NetworkId::of(ship));
    CHECK_THROWS_AS(rtype::spawn_player(registry, tuning::max_players), std::out_of_range);
}

TEST_CASE("a player missile leaves the nose of its shooter", "[rtype][entities]") {
    Registry registry;
    const Entity ship = rtype::spawn_player(registry, 0);
    const Entity missile = rtype::spawn_player_missile(registry, ship);
    CHECK(registry.get<Transform>(missile).position.x > registry.get<Transform>(ship).position.x);
    CHECK(registry.get<Transform>(missile).position.y == registry.get<Transform>(ship).position.y);
    CHECK(registry.get<Velocity>(missile).value == Vec2{tuning::player_missile_speed, 0.0F});
    CHECK(registry.get<Owner>(missile).entity == ship);
    CHECK(registry.get<Damage>(missile).consumed_on_hit);
    CHECK_THROWS_AS(rtype::spawn_player_missile(registry, registry.create()), std::out_of_range);
}

TEST_CASE("Bydos slaves and enemy missiles fly left", "[rtype][entities]") {
    Registry registry;
    const Entity bydos = rtype::spawn_bydos_slave(registry, Vec2{1200.0F, 300.0F});
    const Entity bullet = rtype::spawn_enemy_missile(registry, Vec2{1100.0F, 300.0F});
    CHECK(registry.get<Velocity>(bydos).value.x < 0.0F);
    CHECK(registry.get<Velocity>(bullet).value.x < 0.0F);
    CHECK(registry.get<ScoreValue>(bydos).points == tuning::bydos_score);
    CHECK(registry.get<Health>(bydos).points == tuning::bydos_health);
    CHECK_FALSE(registry.has<Health>(bullet));
}

TEST_CASE("the layer table has no friendly fire", "[rtype][layers]") {
    const Collider player{.layer = layer::player, .mask = mask::player};
    const Collider player_missile{.layer = layer::player_missile, .mask = mask::player_missile};
    const Collider enemy{.layer = layer::enemy, .mask = mask::enemy};
    const Collider enemy_missile{.layer = layer::enemy_missile, .mask = mask::enemy_missile};

    CHECK(hits(player_missile, enemy));
    CHECK(hits(enemy_missile, player));
    CHECK(hits(player, enemy));
    CHECK(hits(enemy, player));
    CHECK_FALSE(hits(player_missile, player));
    CHECK_FALSE(hits(player, player_missile));
    CHECK_FALSE(hits(player, player));
    CHECK_FALSE(hits(enemy, enemy));
    CHECK_FALSE(hits(enemy_missile, enemy));
    CHECK_FALSE(hits(enemy, enemy_missile));
    CHECK_FALSE(hits(player_missile, enemy_missile));
    CHECK_FALSE(hits(enemy_missile, player_missile));
}

TEST_CASE("missiles hit only their targets and enemies ignore enemies", "[rtype][layers]") {
    Registry registry;
    const Vec2 spot{640.0F, 360.0F};
    const Entity one = rtype::spawn_player(registry, 0);
    const Entity two = rtype::spawn_player(registry, 1);
    registry.get<Transform>(one).position = spot;
    registry.get<Transform>(two).position = spot;
    const Entity missile = rtype::spawn_player_missile(registry, one);
    registry.get<Transform>(missile).position = spot;
    const Entity bydos = rtype::spawn_bydos_slave(registry, spot);
    const Entity other_bydos = rtype::spawn_bydos_slave(registry, spot);
    const Entity bullet = rtype::spawn_enemy_missile(registry, spot);

    const std::vector<Contact> contacts = contacts_of(registry);
    CHECK(touching(contacts, missile, bydos));
    CHECK(touching(contacts, missile, other_bydos));
    CHECK(touching(contacts, bullet, one));
    CHECK(touching(contacts, bullet, two));
    CHECK(touching(contacts, one, bydos));
    CHECK_FALSE(touching(contacts, missile, one));
    CHECK_FALSE(touching(contacts, missile, two));
    CHECK_FALSE(touching(contacts, one, two));
    CHECK_FALSE(touching(contacts, bydos, other_bydos));
    CHECK_FALSE(touching(contacts, bullet, bydos));
    CHECK_FALSE(touching(contacts, bullet, missile));
}
