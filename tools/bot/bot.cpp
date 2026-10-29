#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/context.hpp>

#include <rtype/bot.hpp>
#include <rtype/client_session.hpp>
#include <rtype/input_script.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <random>
#include <variant>
#include <vector>

namespace rtype {

namespace ecs = aegis2d::ecs;

namespace {

// Fire is held in this share of the picks: a player shoots most of the time.
constexpr double fire_share = 0.7;

std::variant<RandomInputs, ScriptedInputs> inputs_for(const BotSettings& settings) {
    if (settings.script.has_value()) {
        return ScriptedInputs{*settings.script};
    }
    return RandomInputs{settings.seed};
}

} // namespace

RandomInputs::RandomInputs(std::uint32_t seed) : random_{seed} {}

ecs::PlayerInput RandomInputs::update(Clock::time_point now) {
    if (picks_ > 0 && now < until_) {
        return input_;
    }
    // On each axis: one way, the other, or neither (the third choice).
    constexpr std::array horizontal{ecs::Action::Left, ecs::Action::Right};
    constexpr std::array vertical{ecs::Action::Up, ecs::Action::Down};
    std::uniform_int_distribution<std::size_t> axis{0, 2};
    std::bernoulli_distribution fire{fire_share};
    std::uniform_int_distribution<std::int64_t> hold{shortest_hold.count(), longest_hold.count()};
    input_ = ecs::PlayerInput{};
    if (const std::size_t way = axis(random_); way < horizontal.size()) {
        input_.set(horizontal.at(way), true);
    }
    if (const std::size_t way = axis(random_); way < vertical.size()) {
        input_.set(vertical.at(way), true);
    }
    input_.set(ecs::Action::Fire, fire(random_));
    until_ = now + std::chrono::milliseconds{hold(random_)};
    ++picks_;
    return input_;
}

std::uint64_t RandomInputs::picks() const noexcept {
    return picks_;
}

std::vector<BotSettings> bot_team(const BotSettings& base, std::size_t count) {
    if (count <= 1) {
        return {base};
    }
    std::vector<BotSettings> team(count, base);
    for (std::size_t index = 0; index < count; ++index) {
        team.at(index).server.name = std::format("{}{}", base.server.name, index + 1);
        team.at(index).seed = base.seed + static_cast<std::uint32_t>(index);
    }
    return team;
}

Bot::Bot(aegis2d::net::NetworkContext& context, const BotSettings& settings)
    : session_{context, settings.server}, inputs_{inputs_for(settings)} {}

void Bot::start() {
    session_.start();
}

void Bot::update(Clock::time_point now) {
    if (session_.state() == ClientState::InGame) {
        input_ = std::visit([now](auto& inputs) { return inputs.update(now); }, inputs_);
        session_.set_input(input_);
    }
    session_.update(now);
}

void Bot::leave() {
    session_.leave();
}

const ClientSession& Bot::session() const noexcept {
    return session_;
}

ecs::PlayerInput Bot::input() const noexcept {
    return input_;
}

std::uint64_t Bot::picks() const noexcept {
    // get_if rather than visit, which could throw.
    if (const auto* const random = std::get_if<RandomInputs>(&inputs_)) {
        return random->picks();
    }
    const auto* const scripted = std::get_if<ScriptedInputs>(&inputs_);
    return scripted != nullptr ? scripted->picks() : 0;
}

} // namespace rtype
