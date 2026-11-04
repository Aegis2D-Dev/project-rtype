#include <aegis2d/ecs/collision.hpp>
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>

#include <rtype/lag_compensation.hpp>
#include <rtype/layers.hpp>
#include <rtype/position_history.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <ratio>
#include <vector>

namespace rtype {

namespace ecs = aegis2d::ecs;

std::uint32_t rewind_ticks(std::chrono::nanoseconds rtt,
                           std::chrono::nanoseconds interpolation) noexcept {
    using Ticks = std::chrono::duration<double, std::ratio<1, tuning::tick_rate>>;
    const double ticks = std::chrono::duration_cast<Ticks>(rtt + interpolation).count();
    if (ticks <= 0.0) {
        return 0;
    }
    return static_cast<std::uint32_t>(
        std::min(std::round(ticks), static_cast<double>(PositionHistory::capacity)));
}

void find_rewound_contacts(ecs::Registry& registry, const PositionHistory& history,
                           std::uint64_t tick,
                           const std::function<std::uint32_t(ecs::Entity)>& rewind_of,
                           std::vector<ecs::Contact>& contacts) {
    std::vector<ecs::Contact> found;
    registry.view<ecs::Collider, ecs::Transform, ecs::Owner>().each(
        [&](ecs::Entity missile, const ecs::Collider& collider, const ecs::Transform& transform,
            const ecs::Owner& owner) {
            if (collider.layer != layer::player_missile) {
                return;
            }
            const std::uint32_t rewind = rewind_of(owner.entity);
            if (rewind == 0 || rewind > tick) {
                return;
            }
            const ecs::Aabb box = ecs::Aabb::of(transform, collider);
            for (const auto& [enemy, position] : history.enemies(tick - rewind)) {
                if (!registry.alive(enemy)) {
                    continue;
                }
                const auto target = registry.try_get<ecs::Collider>(enemy);
                if (!target.has_value()) {
                    continue;
                }
                const ecs::Collider& other = target->get();
                if (!box.overlaps(ecs::Aabb::of(ecs::Transform{position}, other))) {
                    continue;
                }
                // As find_contacts() writes them: the smaller handle first.
                const bool missile_first = missile < enemy;
                found.push_back(
                    ecs::Contact{.first = missile_first ? missile : enemy,
                                 .second = missile_first ? enemy : missile,
                                 .first_hits_second = missile_first ? ecs::hits(collider, other)
                                                                    : ecs::hits(other, collider),
                                 .second_hits_first = missile_first ? ecs::hits(other, collider)
                                                                    : ecs::hits(collider, other)});
            }
        });
    contacts.insert(contacts.end(), found.begin(), found.end());
}

} // namespace rtype
