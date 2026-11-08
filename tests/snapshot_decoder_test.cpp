#include <aegis2d/core/clock.hpp>
#include <aegis2d/core/random.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/reliability.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/delta_snapshot.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/snapshot_decoder.hpp>
#include <rtype/snapshot_history.hpp>
#include <rtype/tuning.hpp>
#include <rtype/waves.hpp>
#include <rtype/world_snapshot.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

using rtype::DeltaSnapshot;
using rtype::EntityRecord;
using rtype::PlayerRecord;
using rtype::Snapshot;
using rtype::SnapshotDecoder;
namespace tuning = rtype::tuning;

namespace {

Snapshot world(std::uint32_t tick, std::size_t entities) {
    Snapshot snapshot{.tick = tick,
                      .players = {PlayerRecord{
                          .player = 0, .ship = 1, .lives = 3, .score = 0, .input = std::nullopt}},
                      .entities = {}};
    for (std::uint32_t id = 1; id <= entities; ++id) {
        snapshot.entities.push_back(EntityRecord{.id = id,
                                                 .kind = 3,
                                                 .x = 1000.0F - static_cast<float>(tick),
                                                 .y = static_cast<float>(id),
                                                 .vx = -120.0F,
                                                 .vy = 0.0F,
                                                 .frame = 1});
    }
    return snapshot;
}

// The payloads of a snapshot, read back as the client reads them.
std::vector<DeltaSnapshot> parts_of(const Snapshot& snapshot, const Snapshot* baseline) {
    std::vector<DeltaSnapshot> parts;
    for (const auto& payload : rtype::write_delta_snapshot(snapshot, baseline)) {
        const std::optional<DeltaSnapshot> part = rtype::read_delta_snapshot(payload);
        REQUIRE(part.has_value());
        parts.push_back(part.value_or(DeltaSnapshot{}));
    }
    return parts;
}

// The snapshot as the client must rebuild it: quantized, in the order of the payloads.
Snapshot expected(const Snapshot& snapshot) {
    Snapshot result{.tick = snapshot.tick, .players = snapshot.players, .entities = {}};
    for (const EntityRecord& entity : snapshot.entities) {
        result.entities.push_back(rtype::quantized(entity));
    }
    std::ranges::sort(result.entities, {}, [](const EntityRecord& entity) {
        return ((entity.id & 0xFFFFFFULL) << 32U) | entity.id;
    });
    return result;
}

} // namespace

TEST_CASE("a full snapshot is rebuilt and kept as the baseline of the next",
          "[snapshot][decoder]") {
    SnapshotDecoder decoder;
    const Snapshot first = world(30, 3);
    const std::vector<DeltaSnapshot> full = parts_of(first, nullptr);
    REQUIRE(full.size() == 1);
    CHECK(decoder.decodable(full.front()));
    CHECK(decoder.decode(full.front()) == expected(first));
    REQUIRE(decoder.find(30) != nullptr);
    CHECK(*decoder.find(30) == expected(first));

    const Snapshot second = world(33, 3);
    const std::vector<DeltaSnapshot> delta = parts_of(second, &first);
    REQUIRE(delta.size() == 1);
    CHECK(delta.front().baseline == 30U);
    CHECK(decoder.decode(delta.front()) == expected(second));
    CHECK(decoder.size() == 2);
    CHECK(decoder.dropped() == 0);
}

TEST_CASE("a delta whose baseline is gone is dropped", "[snapshot][decoder]") {
    SnapshotDecoder decoder;
    const Snapshot first = world(3, 2);
    for (std::uint32_t index = 1; index <= SnapshotDecoder::capacity + 1; ++index) {
        const Snapshot snapshot = world(index * 3, 2);
        REQUIRE(decoder.decode(parts_of(snapshot, nullptr).front()));
    }
    // The 33rd pushed the first out.
    CHECK(decoder.size() == SnapshotDecoder::capacity);
    CHECK(decoder.find(3) == nullptr);
    REQUIRE(decoder.find(6) != nullptr);

    const DeltaSnapshot orphan = parts_of(world(102, 2), &first).front();
    CHECK(!decoder.decodable(orphan));
    CHECK(!decoder.decode(orphan));
    CHECK(decoder.find(102) == nullptr);
    CHECK(decoder.dropped() == 1);

    // A baseline never received either.
    const Snapshot never = world(100, 2);
    CHECK(!decoder.decode(parts_of(world(102, 2), &never).front()));
    CHECK(decoder.dropped() == 2);
    // Against a baseline it has, the same tick is rebuilt.
    const Snapshot kept = world(99, 2);
    CHECK(decoder.decode(parts_of(world(102, 2), &kept).front()) == expected(world(102, 2)));
}

TEST_CASE("a snapshot in several datagrams is rebuilt as its parts arrive", "[snapshot][decoder]") {
    SnapshotDecoder decoder;
    const Snapshot base = world(30, 400);
    const std::vector<DeltaSnapshot> full = parts_of(base, nullptr);
    REQUIRE(full.size() > 2);
    for (const DeltaSnapshot& part : full) {
        REQUIRE(decoder.decode(part));
    }
    REQUIRE(decoder.find(30) != nullptr);
    CHECK(*decoder.find(30) == expected(base));

    // Every entity moved: several datagrams, the last arriving first. Until the others
    // arrive, their entities stay as in the baseline.
    const Snapshot moved = world(33, 400);
    const std::vector<DeltaSnapshot> parts = parts_of(moved, &base);
    REQUIRE(parts.size() >= 2);
    const Snapshot first = decoder.decode(parts.back()).value_or(Snapshot{});
    CHECK(first.tick == 33);
    CHECK(first.entities.size() == 400);
    CHECK(first != expected(moved));
    for (std::size_t index = 0; index + 1 < parts.size(); ++index) {
        REQUIRE(decoder.decode(parts.at(index)));
    }
    REQUIRE(decoder.find(33) != nullptr);
    CHECK(*decoder.find(33) == expected(moved));

    // A part of the same tick against another baseline does not belong to it.
    CHECK(!decoder.decode(parts_of(moved, nullptr).front()));
    CHECK(decoder.dropped() == 1);
    CHECK(*decoder.find(33) == expected(moved));
}

TEST_CASE("with 5 % loss the client's world matches the server's at every snapshot",
          "[snapshot][decoder]") {
    rtype::GameInstance instance{7, rtype::load_waves(RTYPE_ASSET_DIR "/waves/stage1.json")};
    for (std::uint8_t player = 0; player < tuning::max_players; ++player) {
        instance.join(player);
        aegis2d::ecs::PlayerInput input;
        input.set(aegis2d::ecs::Action::Fire, true);
        input.set(player % 2 == 0 ? aegis2d::ecs::Action::Up : aegis2d::ecs::Action::Right, true);
        instance.set_input(player, input, player);
    }
    // The server's history of the player, the client's decoder and acknowledgements, and a
    // link losing 5 % of the datagrams each way.
    rtype::SnapshotHistory history;
    SnapshotDecoder decoder;
    aegis2d::net::AckTracker acks;
    aegis2d::core::Random random{284};
    const auto lost = [&random] { return random.below(100) < 5; };
    std::uint16_t sequence = 0;
    std::size_t whole = 0;
    std::size_t deltas = 0;
    std::size_t datagrams = 0;
    std::size_t dropped = 0;
    for (std::uint64_t tick = 1; tick <= 30ULL * tuning::tick_rate; ++tick) {
        instance.game().tick(aegis2d::core::step_from_rate(tuning::tick_rate));
        if (!rtype::snapshot_due(tick)) {
            continue;
        }
        auto snapshot = std::make_shared<const Snapshot>(rtype::take_snapshot(instance));
        const Snapshot* const baseline = history.latest_acknowledged();
        deltas += baseline != nullptr ? 1 : 0;
        std::vector<std::uint16_t> sequences;
        bool received = true;
        for (const auto& payload : rtype::write_delta_snapshot(*snapshot, baseline)) {
            sequences.push_back(sequence++);
            ++datagrams;
            if (lost()) {
                received = false;
                continue;
            }
            const std::optional<DeltaSnapshot> part = rtype::read_delta_snapshot(payload);
            REQUIRE(part.has_value());
            const DeltaSnapshot& read = part.value_or(DeltaSnapshot{});
            // As ClientSession: a datagram it cannot decode is not acknowledged.
            if (!decoder.decodable(read)) {
                received = false;
                ++dropped;
                continue;
            }
            acks.record(sequences.back());
            received = decoder.decode(read).has_value() && received;
        }
        history.sent(snapshot, sequences);
        // Every snapshot the client received whole is the server's, quantized.
        if (received) {
            ++whole;
            REQUIRE(decoder.find(snapshot->tick) != nullptr);
            CHECK(*decoder.find(snapshot->tick) == expected(*snapshot));
        }
        // The client's next Input acknowledges what it received.
        if (!lost()) {
            history.acknowledge(acks.ack(), acks.ack_bits());
        }
    }
    const std::size_t snapshots = 30ULL * tuning::tick_rate / 3;
    CHECK(whole > snapshots * 9 / 10);
    CHECK(deltas > snapshots * 9 / 10);
    CHECK(dropped == decoder.dropped());
    CHECK(datagrams >= snapshots);
}
