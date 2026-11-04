#include <aegis2d/ecs/waves.hpp>

#include <rtype/event_tracker.hpp>
#include <rtype/game_instance.hpp>
#include <rtype/room.hpp>
#include <rtype/simulation.hpp>
#include <rtype/tuning.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace rtype {

Room::Room(std::optional<std::uint64_t> seed, aegis2d::ecs::WaveSchedule waves, std::uint32_t rate,
           TickHandler after_tick)
    : instance_{seed, std::move(waves)},
      simulation_{instance_, rate, [this, after_tick = std::move(after_tick)](GameInstance& game) {
                      if (after_tick) {
                          after_tick(*this, game);
                      }
                  }} {}

GameInstance& Room::instance() noexcept {
    return instance_;
}

Simulation& Room::simulation() noexcept {
    return simulation_;
}

EventTracker& Room::events() noexcept {
    return events_;
}

std::array<std::optional<std::string>, tuning::max_players>& Room::names() noexcept {
    return names_;
}

} // namespace rtype
