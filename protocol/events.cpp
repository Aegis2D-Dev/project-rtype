#include <aegis2d/net/binary.hpp>

#include <rtype/events.hpp>
#include <rtype/login.hpp>
#include <rtype/snapshot.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

namespace {

template <typename... Handlers>
struct Overloaded : Handlers... {
    using Handlers::operator()...;
};

void write_spawn(net::BinaryWriter& writer, const SpawnEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::Spawn));
    writer.write_u32(event.tick);
    writer.write_u32(event.id);
    writer.write_u8(event.kind);
    writer.write_f32(event.x);
    writer.write_f32(event.y);
    writer.write_f32(event.vx);
    writer.write_f32(event.vy);
    writer.write_u8(event.owner);
}

void write_destroy(net::BinaryWriter& writer, const DestroyEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::Destroy));
    writer.write_u32(event.tick);
    writer.write_u32(event.id);
    writer.write_u8(static_cast<std::uint8_t>(event.reason));
    writer.write_f32(event.x);
    writer.write_f32(event.y);
}

void write_death(net::BinaryWriter& writer, const DeathEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::Death));
    writer.write_u32(event.tick);
    writer.write_u8(event.player);
    writer.write_u8(event.lives);
    writer.write_f32(event.x);
    writer.write_f32(event.y);
}

void write_player_joined(net::BinaryWriter& writer, const PlayerJoinedEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::PlayerJoined));
    writer.write_u32(event.tick);
    writer.write_u8(event.player);
    writer.write_string(event.name);
}

void write_player_left(net::BinaryWriter& writer, const PlayerLeftEvent& event) {
    writer.write_u8(static_cast<std::uint8_t>(EventType::PlayerLeft));
    writer.write_u32(event.tick);
    writer.write_u8(event.player);
    writer.write_u8(static_cast<std::uint8_t>(event.reason));
}

bool finite(std::initializer_list<float> values) {
    return std::ranges::all_of(values, [](float value) { return std::isfinite(value); });
}

bool valid_player(std::uint8_t player) noexcept {
    return player < tuning::max_players;
}

std::optional<GameEvent> read_spawn(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint32_t> id = reader.read_u32();
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<float> x = reader.read_f32();
    const std::optional<float> y = reader.read_f32();
    const std::optional<float> vx = reader.read_f32();
    const std::optional<float> vy = reader.read_f32();
    const std::optional<std::uint8_t> owner = reader.read_u8();
    if (!id || !kind || !x || !y || !vx || !vy || !owner || !reader.at_end() ||
        !finite({*x, *y, *vx, *vy}) || (!valid_player(*owner) && *owner != no_player)) {
        return std::nullopt;
    }
    return SpawnEvent{.tick = tick,
                      .id = *id,
                      .kind = *kind,
                      .x = *x,
                      .y = *y,
                      .vx = *vx,
                      .vy = *vy,
                      .owner = *owner};
}

std::optional<GameEvent> read_destroy(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint32_t> id = reader.read_u32();
    const std::optional<std::uint8_t> reason = reader.read_u8();
    const std::optional<float> x = reader.read_f32();
    const std::optional<float> y = reader.read_f32();
    if (!id || !reason || !x || !y || !reader.at_end() || !finite({*x, *y})) {
        return std::nullopt;
    }
    // An unknown reason is kept: the client reads it as Killed (RFC section 4.4).
    return DestroyEvent{
        .tick = tick, .id = *id, .reason = static_cast<DestroyReason>(*reason), .x = *x, .y = *y};
}

std::optional<GameEvent> read_death(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint8_t> player = reader.read_u8();
    const std::optional<std::uint8_t> lives = reader.read_u8();
    const std::optional<float> x = reader.read_f32();
    const std::optional<float> y = reader.read_f32();
    if (!player || !lives || !x || !y || !reader.at_end() || !valid_player(*player) ||
        !finite({*x, *y})) {
        return std::nullopt;
    }
    return DeathEvent{.tick = tick, .player = *player, .lives = *lives, .x = *x, .y = *y};
}

// The name follows the rules of LoginRequest (RFC section 6.4).
std::optional<GameEvent> read_player_joined(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint8_t> player = reader.read_u8();
    std::optional<std::string> name = reader.read_string();
    if (!player || !name || !reader.at_end() || !valid_player(*player) ||
        !valid_player_name(*name)) {
        return std::nullopt;
    }
    return PlayerJoinedEvent{.tick = tick, .player = *player, .name = std::move(*name)};
}

std::optional<GameEvent> read_player_left(net::BinaryReader& reader, std::uint32_t tick) {
    const std::optional<std::uint8_t> player = reader.read_u8();
    const std::optional<std::uint8_t> reason = reader.read_u8();
    if (!player || !reason || !reader.at_end() || !valid_player(*player)) {
        return std::nullopt;
    }
    // An unknown reason is kept: the client reads it as Left (RFC section 4.4).
    return PlayerLeftEvent{
        .tick = tick, .player = *player, .reason = static_cast<LeaveReason>(*reason)};
}

} // namespace

std::vector<std::byte> write_event(const GameEvent& event) {
    net::BinaryWriter writer;
    std::visit(
        Overloaded{[&](const SpawnEvent& spawn) { write_spawn(writer, spawn); },
                   [&](const DestroyEvent& destroy) { write_destroy(writer, destroy); },
                   [&](const DeathEvent& death) { write_death(writer, death); },
                   [&](const PlayerJoinedEvent& joined) { write_player_joined(writer, joined); },
                   [&](const PlayerLeftEvent& left) { write_player_left(writer, left); }},
        event);
    return {writer.data().begin(), writer.data().end()};
}

std::vector<std::vector<std::byte>> write_events(std::span<const GameEvent> events) {
    std::vector<std::vector<std::byte>> encoded;
    encoded.reserve(events.size());
    for (const GameEvent& event : events) {
        encoded.push_back(write_event(event));
    }
    return encoded;
}

std::optional<GameEvent> read_event(std::span<const std::byte> bytes) {
    net::BinaryReader reader{bytes};
    const std::optional<std::uint8_t> type = reader.read_u8();
    const std::optional<std::uint32_t> tick = reader.read_u32();
    if (!type || !tick) {
        return std::nullopt;
    }
    switch (static_cast<EventType>(*type)) {
    case EventType::Spawn:
        return read_spawn(reader, *tick);
    case EventType::Destroy:
        return read_destroy(reader, *tick);
    case EventType::Death:
        return read_death(reader, *tick);
    case EventType::PlayerJoined:
        return read_player_joined(reader, *tick);
    case EventType::PlayerLeft:
        return read_player_left(reader, *tick);
    case EventType::GameOver:
        break;
    }
    return std::nullopt;
}

} // namespace rtype
