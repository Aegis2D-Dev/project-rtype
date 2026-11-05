#include <aegis2d/core/random.hpp>
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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ratio>
#include <string>
#include <utility>
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
    world.apply(
        Snapshot{.tick = 3,
                 .players = {rtype::PlayerRecord{
                     .player = 2, .ship = 11, .lives = 3, .score = 400, .input = std::nullopt}},
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

TEST_CASE("the input a player's record acknowledges only moves forward", "[client][world]") {
    ClientWorld world;
    const auto at = [](std::uint32_t tick, std::optional<std::uint16_t> input) {
        return Snapshot{.tick = tick,
                        .players = {rtype::PlayerRecord{
                            .player = 1, .ship = 4, .lives = 3, .score = 0, .input = input}},
                        .entities = {}};
    };
    const auto input = [&] { return world.players().at(1).value_or(rtype::RemotePlayer{}).input; };
    world.apply(at(3, std::nullopt));
    CHECK(!input().has_value());
    world.apply(at(6, 65534));
    CHECK(input() == std::optional<std::uint16_t>{65534});
    // A late snapshot, or one saying nothing, does not take it back; the wrap moves it on.
    world.apply(at(9, 2));
    world.apply(at(4, 65530));
    world.apply(at(12, std::nullopt));
    CHECK(input() == std::optional<std::uint16_t>{2});
}

TEST_CASE("the client's entities mirror the server's", "[client][world]") {
    aegis2d::net::NetworkContext context;
    rtype::GameServer game_server{context, std::nullopt, aegis2d::ecs::WaveSchedule{}, 0, 0};
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
    rtype::GameInstance& instance = game_server.room().instance();
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

    // Every 16 ms frame, drawn about 100 ms behind: the position goes down by the same step
    // each frame instead of jumping 15 pixels every third frame. The snapshots come on time,
    // so the delay goes down meanwhile, and the drawing runs 1 % fast.
    const auto now = start + 30ms + (interval * 10);
    float previous = 0.0F;
    for (int frame = 0; frame < 6; ++frame) {
        const auto at = now + std::chrono::milliseconds{16 * frame};
        const std::optional<double> tick = world.render_tick(at);
        REQUIRE(tick.has_value());
        const float x = world.position_at(bydos_entity, tick.value_or(0.0)).x;
        if (frame > 0) {
            // 300 px/s.
            CHECK(std::abs((previous - x) - (16.0F * 0.3F * (1.0F + 0.01F))) < 0.01F);
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

namespace {

// When the server ran a tick, from its start.
ClientWorld::Clock::duration at_tick(std::uint32_t tick) {
    return std::chrono::duration_cast<ClientWorld::Clock::duration>(
        std::chrono::duration<double, std::ratio<1, rtype::tuning::tick_rate>>{tick});
}

// A Bydos moving left at 300 px/s, a snapshot every 3 ticks for a minute, each arriving 30 ms
// after it left plus up to jitter more; a frame every 4 ms. Returns the frames at which the
// Bydos drawn did not move, and the delay at the end.
struct Drawn {
    int stops = 0;
    ClientWorld::Clock::duration delay{};
    ClientWorld::Clock::duration longest{};
};

Drawn play(std::uint32_t jitter_ms) {
    ClientWorld world;
    const ClientWorld::Clock::time_point start{};
    world.apply(GameEvent{spawn(0, 7, 2000.0F)}, start);
    aegis2d::core::Random random{295};
    std::vector<std::pair<ClientWorld::Clock::time_point, std::uint32_t>> arrivals;
    for (std::uint32_t tick_index = 3; tick_index <= 3600; tick_index += 3) {
        const auto jitter = std::chrono::milliseconds{random.below(jitter_ms + 1)};
        arrivals.emplace_back(start + at_tick(tick_index) + 30ms + jitter, tick_index);
    }
    std::ranges::sort(arrivals);
    Drawn drawn;
    std::size_t next = 0;
    std::optional<float> previous;
    for (auto now = start; next < arrivals.size(); now += 4ms) {
        while (next < arrivals.size() && arrivals.at(next).first <= now) {
            const std::uint32_t sent = arrivals.at(next).second;
            world.apply(snapshot(sent, 7, 2000.0F - (5.0F * static_cast<float>(sent))),
                        arrivals.at(next).first);
            ++next;
        }
        drawn.longest = std::max(drawn.longest, world.interpolation_delay(now));
        const std::optional<double> at = world.render_tick(now);
        // From the second second on: the first snapshots set the clock.
        if (!at.has_value() || now < start + 1s) {
            continue;
        }
        const float x = world.position_at(world.entity(7).value_or(Entity{}), *at).x;
        if (previous.has_value() && x >= *previous) {
            ++drawn.stops;
        }
        previous = x;
    }
    drawn.delay = world.interpolation_delay(arrivals.back().first);
    return drawn;
}

} // namespace

TEST_CASE("with no jitter the interpolation delay goes down to 50 ms", "[client][world]") {
    const Drawn drawn = play(0);
    CHECK(drawn.delay == ClientWorld::min_interpolation_delay);
    CHECK(drawn.longest == ClientWorld::initial_interpolation_delay);
    CHECK(drawn.stops == 0);
}

TEST_CASE("with 40 ms of jitter no entity stops for lack of snapshots", "[client][world]") {
    const Drawn drawn = play(40);
    const double delay_ms = std::chrono::duration<double, std::milli>{drawn.delay}.count();
    const double longest_ms = std::chrono::duration<double, std::milli>{drawn.longest}.count();
    CAPTURE(delay_ms, longest_ms);
    CHECK(drawn.stops == 0);
    // About 50 ms, plus 1.25 times the 40 ms of the latest snapshots.
    CHECK(drawn.delay > 90ms);
    CHECK(drawn.delay <= ClientWorld::max_interpolation_delay);
}

TEST_CASE("the interpolation delay rises fast and goes down slowly, within its bounds",
          "[client][world]") {
    ClientWorld world;
    const ClientWorld::Clock::time_point start{};
    CHECK(world.interpolation_delay(start) == ClientWorld::initial_interpolation_delay);
    world.apply(snapshot(3, 7, 0.0F), start + at_tick(3));
    // 80 ms late: it aims at 50 + 1.25 x 80 = 150 ms, and gets there at half real time, so
    // the drawing slows down without going back.
    const auto late = start + at_tick(6) + 80ms;
    world.apply(snapshot(6, 7, 0.0F), late);
    const auto before = world.interpolation_delay(late);
    CHECK(before <= ClientWorld::initial_interpolation_delay);
    CHECK(std::chrono::abs(world.interpolation_delay(late + 50ms) - before - 25ms) < 1ms);
    CHECK(std::chrono::abs(world.interpolation_delay(late + 200ms) - 150ms) < 1ms);
    CHECK(world.render_tick(late + 50ms) > world.render_tick(late));
    // 1 s late: 200 ms at most.
    const auto later = start + at_tick(9) + 1s;
    world.apply(snapshot(9, 7, 0.0F), later);
    CHECK(world.interpolation_delay(later + 1s) == ClientWorld::max_interpolation_delay);
    // On time again: the late ones count for 2 s, then 10 ms less per second.
    const auto on_time = [&](std::uint32_t from, std::uint32_t to) {
        for (std::uint32_t index = from; index <= to; ++index) {
            world.apply(snapshot(index * 3, 7, 0.0F), start + at_tick(index * 3));
        }
        return start + at_tick(to * 3);
    };
    auto now = on_time(10, 60); // until 3 s, the window of the 1 s late one
    CHECK(world.interpolation_delay(now) == ClientWorld::max_interpolation_delay);
    now = on_time(61, 120); // 3 s more
    CHECK(world.interpolation_delay(now) < ClientWorld::max_interpolation_delay);
    CHECK(world.interpolation_delay(now) > 160ms);
    now = on_time(121, 600); // 24 s more: down to the floor
    CHECK(world.interpolation_delay(now) == ClientWorld::min_interpolation_delay);
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
