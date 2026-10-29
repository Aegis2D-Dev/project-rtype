#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/context.hpp>

#include <rtype/bot.hpp>
#include <rtype/client_session.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <random>

namespace rtype {

namespace ecs = aegis2d::ecs;

namespace {

// Fire is held in this share of the picks: a player shoots most of the time.
constexpr double fire_share = 0.7;

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

Bot::Bot(aegis2d::net::NetworkContext& context, const BotSettings& settings)
    : session_{context, settings.server}, inputs_{settings.seed} {}

void Bot::start() {
    session_.start();
}

void Bot::update(Clock::time_point now) {
    if (session_.state() == ClientState::InGame) {
        input_ = inputs_.update(now);
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

const RandomInputs& Bot::inputs() const noexcept {
    return inputs_;
}

} // namespace rtype
