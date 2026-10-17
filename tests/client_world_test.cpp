#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/context.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/client_session.hpp>
#include <rtype/client_world.hpp>
#include <rtype/events.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/game_server.hpp>
#include <rtype/server_network.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>
#include <rtype/world_snapshot.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using aegis2d::ecs::Entity;
using aegis2d::ecs::Transform;
using aegis2d::ecs::Velocity;
using rtype::ClientWorld;
using rtype::DestroyEvent;
using rtype::EntityKind;
using rtype::EntityRecord;
using rtype::GameEvent;
using rtype::Snapshot;
using rtype::SpawnEvent;
using rtype::Trail;
using rtype::TrailPoint;
using namespace std::chrono_literals;

namespace {

constexpr auto bydos = static_cast<std::uint8_t>(EntityKind::BydosSlave);

SpawnEvent spawn(std::uint32_t tick, std::uint32_t id, float x) {
    return SpawnEvent{.tick = tick,
                      .id = id,
                      .kind = bydos,
                      .x = x,
                      .y = 100.0F,
                      .vx = -100.0F,
                      .vy = 0.0F,
                      .owner = rtype::no_player};
}

DestroyEvent destroy(std::uint32_t tick, std::uint32_t id) {
    return DestroyEvent{
        .tick = tick, .id = id, .reason = rtype::DestroyReason::Killed, .x = 0.0F, .y = 0.0F};
}

Snapshot snapshot(std::uint32_t tick, std::uint32_t id, float x) {
    return Snapshot{
        .tick = tick,
        .players = {},
        .entities = {EntityRecord{
            .id = id, .kind = bydos, .x = x, .y = 100.0F, .vx = -100.0F, .vy = 0.0F, .frame = 2}}};
}

float x_of(const ClientWorld& world, std::uint32_t id) {
    const std::optional<Entity> entity = world.entity(id);
    REQUIRE(entity.has_value());
    return world.registry().get<Transform>(entity.value_or(Entity{})).position.x;
}

} // namespace

TEST_CASE("events create and destroy entities, snapshots move them", "[client][world]") {
    ClientWorld world;
    const auto now = ClientWorld::Clock::now();
    world.apply(spawn(10, 7, 900.0F), now);
    REQUIRE(world.entities() == 1);
    CHECK(x_of(world, 7) == 900.0F);
    const Entity entity = world.entity(7).value_or(Entity{});
    CHECK(world.registry().get<Velocity>(entity).value.x == -100.0F);
    CHECK(world.registry().get<aegis2d::ecs::Sprite>(entity).size == rtype::tuning::bydos_size);
    CHECK(world.registry().get<rtype::Replicated>(entity).kind == bydos);

    world.apply(snapshot(13, 7, 895.0F));
    CHECK(x_of(world, 7) == 895.0F);
    CHECK(world.registry().get<aegis2d::ecs::Sprite>(entity).frame == 2);
    // RFC section 4.5: an older record arriving late is ignored.
    world.apply(snapshot(12, 7, 896.0F));
    CHECK(x_of(world, 7) == 895.0F);
    // RFC section 4.3: a snapshot never creates an entity.
    world.apply(snapshot(13, 8, 500.0F));
    CHECK(!world.entity(8).has_value());

    world.apply(destroy(20, 7), now);
    CHECK(!world.entity(7).has_value());
    CHECK(world.entities() == 0);
    // A record of the destroyed entity, even a newer one, is ignored.
    world.apply(snapshot(21, 7, 880.0F));
    CHECK(!world.entity(7).has_value());
}

TEST_CASE("a Destroy arriving before its Spawn wins", "[client][world]") {
    ClientWorld world;
    const auto now = ClientWorld::Clock::now();
    world.apply(destroy(30, 5), now);
    world.apply(spawn(25, 5, 900.0F), now);
    CHECK(!world.entity(5).has_value());

    // Ten seconds later the id is forgotten, and may come back for a new entity.
    world.prune(now + ClientWorld::forget_after - 1s);
    world.apply(spawn(26, 5, 900.0F), now);
    CHECK(!world.entity(5).has_value());
    world.prune(now + ClientWorld::forget_after);
    world.apply(spawn(26, 5, 700.0F), now);
    REQUIRE(world.entity(5).has_value());
    CHECK(x_of(world, 5) == 700.0F);
}

TEST_CASE("an id reused by a later Spawn is a new entity", "[client][world]") {
    ClientWorld world;
    const auto now = ClientWorld::Clock::now();
    world.apply(spawn(1, 3, 900.0F), now);
    world.apply(destroy(2, 3), now);
    world.apply(spawn(40, 3, 600.0F), now);
    REQUIRE(world.entity(3).has_value());
    CHECK(x_of(world, 3) == 600.0F);
    CHECK(world.entities() == 1);
}

TEST_CASE("entities of unknown kinds are ignored", "[client][world]") {
    ClientWorld world;
    SpawnEvent unknown = spawn(1, 9, 900.0F);
    unknown.kind = 42;
    world.apply(unknown, ClientWorld::Clock::now());
    CHECK(!world.entity(9).has_value());
    CHECK(world.entities() == 0);
}

TEST_CASE("players come from the snapshots and the events", "[client][world]") {
    ClientWorld world;
    const auto now = ClientWorld::Clock::now();
    world.apply(rtype::PlayerJoinedEvent{.tick = 1, .player = 2, .name = "R9"}, now);
    world.apply(Snapshot{
        .tick = 3,
        .players = {rtype::PlayerRecord{.player = 2, .ship = 11, .lives = 3, .score = 400}},
        .entities = {}});
    REQUIRE(world.players().at(2).has_value());
    const rtype::RemotePlayer player = world.players().at(2).value_or(rtype::RemotePlayer{});
    CHECK(player.name == "R9");
    CHECK(player.ship == 11);
    CHECK(player.lives == 3);
    CHECK(player.score == 400);

    world.apply(rtype::DeathEvent{.tick = 5, .player = 2, .lives = 2, .x = 0.0F, .y = 0.0F}, now);
    CHECK(world.players().at(2).value_or(rtype::RemotePlayer{}).lives == 2);
    world.apply(rtype::PlayerLeftEvent{.tick = 6, .player = 2, .reason = rtype::LeaveReason::Left},
                now);
    CHECK(!world.players().at(2).has_value());
    CHECK(!world.players().at(0).has_value());
}

TEST_CASE("the client's entities mirror the server's", "[client][world]") {
    aegis2d::net::NetworkContext context;
    rtype::GameInstance instance{std::nullopt, aegis2d::ecs::WaveSchedule{}};
    rtype::GameServer game_server{context, instance, 0, 0};
    rtype::ServerNetwork& server = game_server.network();
    rtype::ClientSession client{
        context, rtype::ClientSettings{
                     .host = "127.0.0.1", .tcp_port = server.tcp_endpoint().port(), .name = "R9"}};
    const auto run = [&](std::chrono::milliseconds duration) {
        const auto end = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < end) {
            context.run_for(std::chrono::milliseconds{1});
            const auto now = rtype::ClientSession::Clock::now();
            server.update(now);
            client.update(now);
        }
    };
    client.start();
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (client.state() != rtype::ClientState::InGame &&
           std::chrono::steady_clock::now() < deadline) {
        run(std::chrono::milliseconds{10});
    }
    REQUIRE(client.state() == rtype::ClientState::InGame);

    // The player flies right and fires: its missiles appear, fly and leave the screen.
    aegis2d::ecs::PlayerInput held;
    held.set(aegis2d::ecs::Action::Right, true);
    held.set(aegis2d::ecs::Action::Fire, true);
    client.set_input(held);
    run(std::chrono::milliseconds{1500});
    // The game stops; what it already posted reaches the client.
    game_server.simulation().stop();
    run(std::chrono::milliseconds{300});

    const Snapshot truth = rtype::take_snapshot(instance);
    const rtype::ClientWorld& world = client.world();
    CAPTURE(truth.entities.size(), world.entities());
    REQUIRE(truth.entities.size() > 1);
    CHECK(world.entities() == truth.entities.size());
    for (const EntityRecord& record : truth.entities) {
        CAPTURE(record.id, record.kind);
        const std::optional<Entity> entity = world.entity(record.id);
        REQUIRE(entity.has_value());
        const Entity local = entity.value_or(Entity{});
        CHECK(world.registry().get<rtype::Replicated>(local).kind == record.kind);
        // The last snapshot is at most two ticks older than the game's end: a missile moves
        // 15 pixels per tick.
        const auto position = world.registry().get<Transform>(local).position;
        CHECK(std::abs(position.x - record.x) <= 40.0F);
        CHECK(std::abs(position.y - record.y) <= 40.0F);
    }
    REQUIRE(world.players().at(0).has_value());
    const rtype::RemotePlayer me = world.players().at(0).value_or(rtype::RemotePlayer{});
    CHECK(me.name == "R9");
    CHECK(me.ship == truth.players.at(0).ship);
}

TEST_CASE("interpolate draws an entity between the positions of its trail", "[client][world]") {
    Trail trail;
    CHECK(rtype::interpolate(trail, 10.0).x == 0.0F);

    trail.push(TrailPoint{.tick = 0, .position = {0.0F, 10.0F}});
    trail.push(TrailPoint{.tick = 3, .position = {30.0F, 40.0F}});
    trail.push(TrailPoint{.tick = 6, .position = {30.0F, 100.0F}});
    trail.push(TrailPoint{.tick = 6, .position = {999.0F, 999.0F}}); // same tick: ignored
    REQUIRE(trail.count == 3);

    CHECK(rtype::interpolate(trail, -5.0).x == 0.0F); // before the oldest: the oldest
    CHECK(std::abs(rtype::interpolate(trail, 1.5).x - 15.0F) < 0.001F);
    CHECK(std::abs(rtype::interpolate(trail, 1.5).y - 25.0F) < 0.001F);
    CHECK(std::abs(rtype::interpolate(trail, 4.0).y - 60.0F) < 0.001F);
    CHECK(rtype::interpolate(trail, 50.0).y == 100.0F); // after the newest: the newest

    for (std::uint32_t tick = 9; tick <= 18; tick += 3) {
        trail.push(TrailPoint{.tick = tick, .position = {static_cast<float>(tick), 0.0F}});
    }
    CHECK(trail.count == Trail::capacity);
    CHECK(trail.points.front().tick == 9);
}

TEST_CASE("remote entities move smoothly between snapshots sent at 20 Hz", "[client][world]") {
    ClientWorld world;
    const ClientWorld::Clock::time_point start{};
    world.apply(GameEvent{spawn(0, 7, 600.0F)}, start);
    CHECK_FALSE(world.render_tick(start).has_value());

    // A snapshot every 3 ticks (20 Hz), the Bydos moving 5 pixels a tick, 30 ms on the way.
    constexpr auto interval = std::chrono::milliseconds{50};
    for (std::uint32_t index = 1; index <= 10; ++index) {
        const std::uint32_t tick = index * 3;
        world.apply(snapshot(tick, 7, 600.0F - (5.0F * static_cast<float>(tick))),
                    start + 30ms + (interval * index));
    }
    const Entity bydos_entity = world.entity(7).value_or(Entity{});

    // Every 16 ms frame, drawn 100 ms behind: the position goes down by the same step each
    // frame instead of jumping 15 pixels every third frame.
    const auto now = start + 30ms + (interval * 10);
    float previous = 0.0F;
    for (int frame = 0; frame < 6; ++frame) {
        const auto at = now + std::chrono::milliseconds{16 * frame};
        const std::optional<double> tick = world.render_tick(at);
        REQUIRE(tick.has_value());
        const float x = world.position_at(bydos_entity, tick.value_or(0.0)).x;
        if (frame > 0) {
            CHECK(std::abs((previous - x) - (16.0F * 0.3F)) < 0.01F); // 300 px/s
        }
        previous = x;
    }

    // A late datagram does not move the clock back.
    const std::optional<double> before = world.server_tick(now);
    world.apply(snapshot(33, 7, 435.0F), now + 400ms);
    const std::optional<double> after = world.server_tick(now);
    REQUIRE(before.has_value());
    REQUIRE(after.has_value());
    CHECK(std::abs(before.value_or(0.0) - after.value_or(0.0)) < 1.0);
}

TEST_CASE("the applied events are handed out once, the ignored ones are not", "[client][world]") {
    ClientWorld world;
    const ClientWorld::Clock::time_point now{};
    world.apply(GameEvent{spawn(10, 7, 600.0F)}, now);
    world.apply(GameEvent{destroy(20, 7)}, now);
    world.apply(GameEvent{spawn(15, 7, 600.0F)}, now); // late Spawn after its Destroy
    world.apply(GameEvent{rtype::PlayerJoinedEvent{.tick = 21, .player = 1, .name = "Lolo"}}, now);

    const std::vector<GameEvent> events = world.take_events();
    REQUIRE(events.size() == 3);
    CHECK(std::holds_alternative<SpawnEvent>(events.at(0)));
    CHECK(std::holds_alternative<DestroyEvent>(events.at(1)));
    CHECK(std::holds_alternative<rtype::PlayerJoinedEvent>(events.at(2)));
    CHECK(world.take_events().empty());
}
