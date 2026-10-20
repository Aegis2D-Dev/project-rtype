#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/shmup/components.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <rtype/entities.hpp>
#include <rtype/server_game.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

using aegis2d::Vec2;
using aegis2d::core::step_from_rate;
using aegis2d::ecs::Action;
using aegis2d::ecs::Entity;
using aegis2d::ecs::Lives;
using aegis2d::ecs::PlayerInput;
using aegis2d::ecs::Score;
using aegis2d::ecs::SineMotion;
using aegis2d::ecs::Transform;
using aegis2d::ecs::Velocity;
using aegis2d::shmup::Weapon;
using Catch::Approx;
using rtype::ServerGame;
namespace tuning = rtype::tuning;

namespace {

void run(ServerGame& game, int ticks) {
    for (int tick = 0; tick < ticks; ++tick) {
        game.tick(step_from_rate(tuning::tick_rate));
    }
}

PlayerInput holding(Action action) {
    PlayerInput input;
    input.set(action, true);
    return input;
}

Vec2 position_of(const ServerGame& game, Entity entity) {
    return game.registry().get<Transform>(entity).position;
}

void place(ServerGame& game, Entity entity, Vec2 position) {
    game.registry().get<Transform>(entity).position = position;
}

// Tick and position of each Bydos spawned at random during the first ticks of a game.
std::vector<std::pair<std::uint64_t, Vec2>> spawns_of(ServerGame& game, int ticks) {
    std::vector<std::pair<std::uint64_t, Vec2>> spawns;
    for (int tick = 0; tick < ticks; ++tick) {
        run(game, 1);
        for (const Entity bydos : game.spawned()) {
            spawns.emplace_back(game.ticks(), position_of(game, bydos));
        }
    }
    return spawns;
}

// A Bydos that does not weave, for the tests that aim a missile at it.
Entity straight_bydos(ServerGame& game, Vec2 position) {
    const Entity bydos = rtype::spawn_bydos_slave(game.registry(), position);
    game.registry().remove<SineMotion>(bydos);
    return bydos;
}

} // namespace

TEST_CASE("the server registers its systems in tick order", "[rtype][server]") {
    const ServerGame game;
    CHECK(game.systems().names() ==
          std::vector<std::string_view>{"steering", "movement", "sine_motion", "play_area",
                                        "weapons", "enemy_fire", "spawn", "waves", "collision",
                                        "damage", "score", "lives", "cleanup", "despawn"});
    CHECK(game.ticks() == 0);
    CHECK(game.players() == 0);
    CHECK_FALSE(game.game_over());
}

TEST_CASE("up to four players join in their own slots", "[rtype][server]") {
    ServerGame game;
    std::vector<Entity> ships;
    for (std::size_t slot = 0; slot < tuning::max_players; ++slot) {
        ships.push_back(game.add_player());
        CHECK(game.slot_of(ships.back()) == slot);
        CHECK(position_of(game, ships.back()) == rtype::player_spawn(slot));
    }
    CHECK(game.players() == 4);
    CHECK_THROWS_AS(game.add_player(), std::length_error);
    CHECK_FALSE(game.slot_of(game.registry().create()));
}

TEST_CASE("players join the slot of their player number and can leave", "[rtype][server]") {
    ServerGame game;
    const Entity three = game.add_player(3);
    const Entity one = game.add_player(1);
    CHECK(game.slot_of(three) == 3);
    CHECK(game.slot_of(one) == 1);
    CHECK(position_of(game, three) == rtype::player_spawn(3));
    CHECK_THROWS_AS(game.add_player(1), std::logic_error);
    CHECK_THROWS_AS(game.add_player(tuning::max_players), std::out_of_range);
    CHECK(game.slot_of(game.add_player()) == 0);

    CHECK(game.remove_player(one));
    CHECK(!game.registry().alive(one));
    CHECK(!game.remove_player(one));
    CHECK(!game.set_input(one, holding(Action::Fire)));
    CHECK(game.players() == 2);
    CHECK(!game.game_over());
    CHECK(game.slot_of(game.add_player(1)) == 1);
}

TEST_CASE("a player ship moves at a fixed speed per second", "[rtype][server][131]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Vec2 start = position_of(game, ship);
    CHECK(game.set_input(ship, holding(Action::Down)));

    run(game, static_cast<int>(tuning::tick_rate / 2));
    CHECK(position_of(game, ship).y == Approx(start.y + (tuning::player_speed / 2.0F)));
    CHECK(position_of(game, ship).x == Approx(start.x));
    CHECK(game.ticks() == tuning::tick_rate / 2);
}

TEST_CASE("holding an arrow stops at the edge", "[rtype][server][132]") {
    ServerGame game;
    const Entity ship = game.add_player();

    game.set_input(ship, holding(Action::Right));
    run(game, 10 * static_cast<int>(tuning::tick_rate));
    CHECK(position_of(game, ship).x == Approx(tuning::area_width - (tuning::player_size.x / 2.0F)));

    game.set_input(ship, holding(Action::Up));
    run(game, 10 * static_cast<int>(tuning::tick_rate));
    CHECK(position_of(game, ship).y == Approx(tuning::player_size.y / 2.0F));

    game.set_input(ship, holding(Action::Left));
    run(game, 10 * static_cast<int>(tuning::tick_rate));
    CHECK(position_of(game, ship).x == Approx(tuning::player_size.x / 2.0F));
}

TEST_CASE("a missile kills a Bydos and credits its shooter", "[rtype][server][140][141]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Entity bydos = straight_bydos(game, Vec2{400.0F, position_of(game, ship).y});
    const Entity missile = rtype::spawn_player_missile(game.registry(), ship);

    bool killed = false;
    for (int tick = 0; tick < 60 && !killed; ++tick) {
        run(game, 1);
        killed = !game.registry().alive(bydos);
    }
    REQUIRE(killed);
    CHECK_FALSE(game.registry().alive(missile));
    CHECK(game.registry().get<Score>(ship).points == tuning::bydos_score);
    REQUIRE(game.awards().size() == 1);
    CHECK(game.awards().front().player == ship);
    CHECK(game.damage().kills.size() == 1);
}

TEST_CASE("each player's score increases on their own kills", "[rtype][server][141]") {
    ServerGame game;
    const Entity one = game.add_player();
    const Entity two = game.add_player();
    const auto bydos_in_front_of = [&game](Entity ship, float x) {
        return straight_bydos(game, Vec2{x, position_of(game, ship).y});
    };
    bydos_in_front_of(one, 500.0F);
    bydos_in_front_of(two, 500.0F);
    bydos_in_front_of(two, 900.0F);
    rtype::spawn_player_missile(game.registry(), one);
    rtype::spawn_player_missile(game.registry(), two);
    run(game, 30);
    rtype::spawn_player_missile(game.registry(), two);
    run(game, 60);

    CHECK(game.registry().get<Score>(one).points == tuning::bydos_score);
    CHECK(game.registry().get<Score>(two).points == 2 * tuning::bydos_score);
}

TEST_CASE("enemies and missiles of the same side pass through each other", "[rtype][server][139]") {
    ServerGame game;
    const Entity one = game.add_player();
    const Entity two = game.add_player();
    place(game, two, position_of(game, one));
    const Entity first = rtype::spawn_bydos_slave(game.registry(), Vec2{900.0F, 300.0F});
    const Entity second = rtype::spawn_bydos_slave(game.registry(), Vec2{900.0F, 300.0F});
    const Entity bullet = rtype::spawn_enemy_missile(game.registry(), Vec2{905.0F, 300.0F});
    const Entity missile = rtype::spawn_player_missile(game.registry(), one);

    run(game, 1);
    CHECK(game.registry().alive(one));
    CHECK(game.registry().alive(two));
    CHECK(game.registry().alive(first));
    CHECK(game.registry().alive(second));
    CHECK(game.registry().alive(bullet));
    CHECK(game.registry().alive(missile));
    CHECK(game.damage().hits.empty());
}

TEST_CASE("a hit player respawns until the game is over", "[rtype][server][142]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Vec2 spawn = rtype::player_spawn(0);
    game.set_input(ship, holding(Action::Right));

    for (std::uint32_t life = tuning::player_lives; life > 0; --life) {
        REQUIRE(game.players() == 1);
        run(game, 20);
        rtype::spawn_enemy_missile(game.registry(), position_of(game, ship));
        run(game, 1);
        REQUIRE(game.lives_lost().size() == 1);
        CHECK(game.lives_lost().front().lives_left == life - 1);
        if (life > 1) {
            CHECK(position_of(game, ship) == spawn);
        }
    }
    CHECK_FALSE(game.registry().alive(ship));
    CHECK(game.players() == 0);
    CHECK(game.game_over());
    CHECK_FALSE(game.set_input(ship, holding(Action::Left)));
}

TEST_CASE("the game is over only when every player is out", "[rtype][server][142]") {
    ServerGame game;
    const Entity one = game.add_player();
    const Entity two = game.add_player();
    const auto shoot = [&game](Entity ship, int times) {
        for (int shot = 0; shot < times; ++shot) {
            rtype::spawn_enemy_missile(game.registry(), position_of(game, ship));
            run(game, 1);
        }
    };

    shoot(one, static_cast<int>(tuning::player_lives));
    CHECK_FALSE(game.registry().alive(one));
    CHECK_FALSE(game.game_over());
    CHECK(game.players() == 1);

    const Entity back = game.add_player();
    CHECK(game.slot_of(back) == 0);
    shoot(two, static_cast<int>(tuning::player_lives));
    CHECK_FALSE(game.game_over());
    shoot(back, static_cast<int>(tuning::player_lives));
    CHECK(game.game_over());
}

TEST_CASE("ramming a Bydos costs a life and kills it", "[rtype][server][140][142]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Entity bydos = rtype::spawn_bydos_slave(game.registry(), Vec2{});
    place(game, bydos, position_of(game, ship));

    run(game, 1);
    CHECK_FALSE(game.registry().alive(bydos));
    REQUIRE(game.lives_lost().size() == 1);
    CHECK(game.lives_lost().front().lives_left == tuning::player_lives - 1);
    CHECK(game.registry().get<Score>(ship).points == tuning::bydos_score);
}

TEST_CASE("holding fire respects the rate", "[rtype][server][133]") {
    ServerGame game;
    const Entity ship = game.add_player();
    game.set_input(ship, holding(Action::Fire));

    std::size_t missiles = 0;
    for (std::uint32_t tick = 0; tick < 10 * tuning::tick_rate; ++tick) {
        run(game, 1);
        missiles += game.fired().size();
        for (const Entity missile : game.fired()) {
            CHECK(game.registry().get<aegis2d::ecs::Owner>(missile).entity == ship);
        }
    }
    const auto expected = static_cast<std::size_t>(10.0F / tuning::player_fire_cooldown);
    CHECK(missiles >= expected);
    CHECK(missiles <= expected + 1);
}

TEST_CASE("missiles leave the ship's nose and fly right", "[rtype][server][133]") {
    ServerGame game;
    const Entity ship = game.add_player();
    game.set_input(ship, holding(Action::Fire));
    run(game, 1);
    REQUIRE(game.fired().size() == 1);
    const Entity missile = game.fired().front();
    CHECK(position_of(game, missile).x > position_of(game, ship).x);
    CHECK(position_of(game, missile).y == position_of(game, ship).y);

    const float before = position_of(game, missile).x;
    run(game, 1);
    CHECK(game.fired().empty());
    CHECK(position_of(game, missile).x > before);
}

TEST_CASE("tapping fire does not shoot faster", "[rtype][server][133]") {
    ServerGame game;
    const Entity ship = game.add_player();
    std::size_t missiles = 0;
    for (std::uint32_t tick = 0; tick < 10 * tuning::tick_rate; ++tick) {
        game.set_input(ship, tick % 2 == 0 ? holding(Action::Fire) : PlayerInput{});
        run(game, 1);
        missiles += game.fired().size();
    }
    CHECK(missiles <= static_cast<std::size_t>(10.0F / tuning::player_fire_cooldown) + 1);
}

TEST_CASE("each player fires their own missiles", "[rtype][server][133]") {
    ServerGame game;
    const Entity shooter = game.add_player();
    const Entity idle = game.add_player();
    game.set_input(shooter, holding(Action::Fire));
    run(game, static_cast<int>(tuning::tick_rate));
    CHECK(game.registry().count<aegis2d::ecs::Owner>() == 4);
    bool idle_fired = false;
    game.registry().view<aegis2d::ecs::Owner>().each(
        [&idle_fired, idle](const aegis2d::ecs::Owner& owner) {
            idle_fired = idle_fired || owner.entity == idle;
        });
    CHECK_FALSE(idle_fired);
}

TEST_CASE("a fired missile kills a Bydos in its path", "[rtype][server][133][141]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Entity bydos = straight_bydos(game, Vec2{600.0F, position_of(game, ship).y});
    game.set_input(ship, holding(Action::Fire));

    run(game, static_cast<int>(tuning::tick_rate));
    CHECK_FALSE(game.registry().alive(bydos));
    CHECK(game.registry().get<Score>(ship).points == tuning::bydos_score);
}

TEST_CASE("a missile that leaves the screen is despawned and reported", "[rtype][server][134]") {
    ServerGame game;
    const Entity ship = game.add_player();
    game.set_input(ship, holding(Action::Fire));
    run(game, 1);
    REQUIRE(game.fired().size() == 1);
    const Entity missile = game.fired().front();
    game.set_input(ship, PlayerInput{});

    const float gone_at =
        tuning::area_width + tuning::despawn_margin + (tuning::player_missile_size.x / 2.0F);
    const float step = tuning::player_missile_speed / static_cast<float>(tuning::tick_rate);
    float last = position_of(game, missile).x;
    int ticks = 0;
    while (game.registry().alive(missile)) {
        REQUIRE(ticks < 2 * static_cast<int>(tuning::tick_rate));
        CHECK(game.despawned().empty());
        last = position_of(game, missile).x;
        run(game, 1);
        ++ticks;
    }
    CHECK(game.despawned() == std::vector<Entity>{missile});
    CHECK(last < gone_at);
    CHECK(last + step >= gone_at);
    run(game, 1);
    CHECK(game.despawned().empty());
}

TEST_CASE("holding fire for a minute does not leak missiles", "[rtype][server][134]") {
    ServerGame game;
    const Entity ship = game.add_player();
    game.set_input(ship, holding(Action::Fire));

    std::size_t fired = 0;
    std::size_t despawned = 0;
    std::size_t peak = 0;
    for (std::uint32_t tick = 0; tick < 60 * tuning::tick_rate; ++tick) {
        run(game, 1);
        fired += game.fired().size();
        despawned += game.despawned().size();
        peak = std::max(peak, game.registry().size());
    }
    CHECK(fired >= 239);
    CHECK(game.registry().size() == 1 + fired - despawned);
    const float lifetime =
        (tuning::area_width + tuning::despawn_margin) / tuning::player_missile_speed;
    const auto in_flight = static_cast<std::size_t>(lifetime / tuning::player_fire_cooldown) + 1;
    CHECK(peak <= 1 + in_flight);
}

TEST_CASE("Bydos and enemy missiles leave on the left", "[rtype][server][134]") {
    ServerGame game;
    const Entity bydos =
        rtype::spawn_bydos_slave(game.registry(), Vec2{tuning::area_width + 40.0F, 200.0F});
    const Entity bullet = rtype::spawn_enemy_missile(game.registry(), Vec2{300.0F, 600.0F});

    run(game, 2 * static_cast<int>(tuning::tick_rate));
    CHECK_FALSE(game.registry().alive(bullet));
    CHECK(game.registry().alive(bydos));

    run(game, 12 * static_cast<int>(tuning::tick_rate));
    CHECK_FALSE(game.registry().alive(bydos));
    CHECK(game.registry().size() == 0);
}

TEST_CASE("players are never despawned", "[rtype][server][134]") {
    ServerGame game;
    const Entity ship = game.add_player();
    place(game, ship, Vec2{-500.0F, -500.0F});
    run(game, 1);
    CHECK(game.registry().alive(ship));
    CHECK(game.despawned().empty());
}

TEST_CASE("Bydos slaves weave around their line as they fly left", "[rtype][server][135]") {
    ServerGame game;
    const Vec2 start{1000.0F, 360.0F};
    const Entity bydos = rtype::spawn_bydos_slave(game.registry(), start);
    const int wave = static_cast<int>(tuning::bydos_wave_period * tuning::tick_rate);
    const auto x_after = [&start](float seconds) {
        return start.x - (tuning::bydos_speed * seconds);
    };

    run(game, wave / 4);
    CHECK(position_of(game, bydos).x == Approx(x_after(0.5F)).margin(0.01));
    CHECK(position_of(game, bydos).y ==
          Approx(start.y - tuning::bydos_wave_amplitude).margin(0.01));
    run(game, wave / 2);
    CHECK(position_of(game, bydos).x == Approx(x_after(1.5F)).margin(0.01));
    CHECK(position_of(game, bydos).y ==
          Approx(start.y + tuning::bydos_wave_amplitude).margin(0.01));
    run(game, wave / 4);
    CHECK(position_of(game, bydos).x == Approx(x_after(2.0F)).margin(0.01));
    CHECK(position_of(game, bydos).y == Approx(start.y).margin(0.01));
}

TEST_CASE("a Bydos weaving above the screen comes back", "[rtype][server][134][135]") {
    ServerGame game;
    const Entity bydos = rtype::spawn_bydos_slave(game.registry(), Vec2{1000.0F, 0.0F});
    const int wave = static_cast<int>(tuning::bydos_wave_period * tuning::tick_rate);

    float highest = 0.0F;
    for (int tick = 0; tick < wave; ++tick) {
        run(game, 1);
        REQUIRE(game.registry().alive(bydos));
        highest = std::min(highest, position_of(game, bydos).y);
    }
    CHECK(highest + (tuning::bydos_size.y / 2.0F) < 0.0F);
    CHECK(position_of(game, bydos).y == Approx(0.0F).margin(0.01));
}

TEST_CASE("without a seed, no enemy spawns", "[rtype][server][136]") {
    ServerGame game;
    CHECK_FALSE(game.seed().has_value());
    CHECK_FALSE(game.systems().enabled("spawn"));
    run(game, 60 * 10);
    CHECK(game.spawned().empty());
    CHECK(game.registry().size() == 0);
}

TEST_CASE("a fixed seed gives the same spawns", "[rtype][server][136]") {
    ServerGame first{42};
    ServerGame second{42};
    ServerGame other{43};
    CHECK(first.seed() == 42);
    CHECK(first.systems().enabled("spawn"));
    const int minute = 60 * 60;
    const auto spawns = spawns_of(first, minute);
    CHECK(spawns.size() >= 30);
    CHECK(spawns == spawns_of(second, minute));
    CHECK(spawns != spawns_of(other, minute));
}

TEST_CASE("random Bydos enter on the right and weave on screen", "[rtype][server][135][136]") {
    ServerGame game{7};
    std::size_t spawned = 0;
    std::size_t despawned = 0;
    std::size_t enemy_shots = 0;
    std::uint64_t last_spawn = 0;
    const auto ticks_of = [](float seconds) {
        return static_cast<std::uint64_t>(seconds * static_cast<float>(tuning::tick_rate));
    };
    for (int tick = 0; tick < 60 * 60 * 2; ++tick) {
        run(game, 1);
        for (const Entity bydos : game.spawned()) {
            CHECK(position_of(game, bydos).x == tuning::bydos_spawn_x);
            if (spawned > 0) {
                const std::uint64_t gap = game.ticks() - last_spawn;
                REQUIRE(gap + 1 >= ticks_of(tuning::bydos_spawn_min_interval));
                REQUIRE(gap <= ticks_of(tuning::bydos_spawn_max_interval) + 1);
            }
            last_spawn = game.ticks();
            ++spawned;
        }
        despawned += game.despawned().size();
        enemy_shots += game.enemy_fired().size();
        game.registry().view<SineMotion, Transform>().each(
            [](const SineMotion& /*wave*/, const Transform& transform) {
                REQUIRE(transform.position.y - (tuning::bydos_size.y / 2.0F) >= -0.01F);
                REQUIRE(transform.position.y + (tuning::bydos_size.y / 2.0F) <=
                        tuning::area_height + 0.01F);
            });
    }
    CHECK(spawned >= 60);
    CHECK(despawned > 0);
    CHECK(enemy_shots > spawned);
    CHECK(game.registry().size() == spawned + enemy_shots - despawned);
}

TEST_CASE("a Bydos fires to the left every two seconds", "[rtype][server][137]") {
    ServerGame game;
    const Entity bydos = straight_bydos(game, Vec2{1000.0F, 360.0F});
    const auto rate = static_cast<std::uint64_t>(tuning::tick_rate);
    std::vector<std::uint64_t> shots;
    for (std::uint64_t tick = 0; tick < 6 * rate; ++tick) {
        run(game, 1);
        for (const Entity missile : game.enemy_fired()) {
            shots.push_back(game.ticks());
            const Vec2 from = position_of(game, bydos);
            CHECK(position_of(game, missile).x < from.x - (tuning::bydos_size.x / 2.0F));
            CHECK(position_of(game, missile).y == from.y);
            CHECK(game.registry().get<Velocity>(missile).value ==
                  Vec2{-tuning::enemy_missile_speed, 0.0F});
        }
    }
    const auto at = [](float seconds) {
        return static_cast<std::uint64_t>(seconds * static_cast<float>(tuning::tick_rate));
    };
    REQUIRE(shots.size() == 3);
    for (std::size_t shot = 0; shot < shots.size(); ++shot) {
        const std::uint64_t expected =
            at(tuning::bydos_first_shot + (static_cast<float>(shot) * tuning::bydos_fire_cooldown));
        CHECK(shots.at(shot) + 1 >= expected);
        CHECK(shots.at(shot) <= expected + 1);
    }
    CHECK(game.fired().empty());
}

TEST_CASE("enemy missiles can hit players", "[rtype][server][137][142]") {
    ServerGame game;
    const Entity ship = game.add_player();
    const Vec2 start = position_of(game, ship);
    const Entity bydos = straight_bydos(game, Vec2{start.x + 400.0F, start.y});
    game.registry().get<Weapon>(bydos).reload = 0.0F;

    run(game, 1);
    REQUIRE(game.enemy_fired().size() == 1);
    const Entity missile = game.enemy_fired().front();
    bool hit = false;
    for (int tick = 0; tick < 2 * 60 && !hit; ++tick) {
        run(game, 1);
        hit = !game.lives_lost().empty();
    }
    REQUIRE(hit);
    CHECK(game.lives_lost().front().player == ship);
    CHECK_FALSE(game.registry().alive(missile));
    CHECK(game.registry().get<Lives>(ship).count == tuning::player_lives - 1);
    CHECK(game.registry().alive(bydos));
}

TEST_CASE("player missiles and enemy missiles pass through each other", "[rtype][server][137]") {
    ServerGame game;
    const Vec2 line{640.0F, 360.0F};
    const Entity enemy_missile = rtype::spawn_enemy_missile(game.registry(), line);
    const Entity bydos = straight_bydos(game, Vec2{1200.0F, 600.0F});
    const Entity player_missile = rtype::spawn_player_missile(game.registry(), bydos);
    place(game, player_missile, line);
    run(game, 1);
    CHECK(game.registry().alive(enemy_missile));
    CHECK(game.registry().alive(player_missile));
    CHECK(game.damage().kills.empty());
}

TEST_CASE("random Bydos fire at their own times, the same for a seed",
          "[rtype][server][136][137]") {
    const auto shots_of = [](ServerGame& game) {
        std::vector<std::pair<std::uint64_t, Vec2>> shots;
        for (int tick = 0; tick < 60 * 30; ++tick) {
            run(game, 1);
            for (const Entity missile : game.enemy_fired()) {
                shots.emplace_back(game.ticks(), position_of(game, missile));
            }
        }
        return shots;
    };
    ServerGame first{42};
    ServerGame second{42};
    const auto shots = shots_of(first);
    CHECK(shots.size() >= 20);
    CHECK(shots == shots_of(second));
    std::size_t same_tick = 0;
    for (std::size_t index = 1; index < shots.size(); ++index) {
        same_tick += shots.at(index).first == shots.at(index - 1).first ? 1U : 0U;
    }
    CHECK(same_tick * 4 < shots.size());
}

TEST_CASE("enemies off screen do not fire", "[rtype][server][137]") {
    ServerGame game;
    const Entity leaving = straight_bydos(game, Vec2{-10.0F, 360.0F});
    const Entity coming = straight_bydos(game, Vec2{tuning::bydos_spawn_x + 200.0F, 200.0F});
    game.registry().get<Weapon>(leaving).reload = 0.0F;
    game.registry().get<Weapon>(coming).reload = 0.0F;
    run(game, 1);
    CHECK(game.enemy_fired().empty());
    CHECK(game.registry().size() == 2);
}
