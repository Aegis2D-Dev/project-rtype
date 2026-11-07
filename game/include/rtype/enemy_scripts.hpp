/// @file
/// @brief Enemy types written in Lua: their values and their behavior, run by the server.
#pragma once
#include <aegis2d/core/math.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/registry.hpp>
#include <aegis2d/script/state.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

/// @brief Values of an enemy type, as its script gives them to `enemy { ... }`.
struct EnemyType {
    /// @brief Name of the type: its file name without `.lua`, as waves files name it.
    std::string name;
    /// @brief Size on screen, also its hit box (`width` and `height`, 32 by default).
    aegis2d::Vec2 size{32.0F, 32.0F};
    /// @brief Health points (`health`, 1 by default).
    std::int32_t health = 1;
    /// @brief Points for its killer (`score`, 100 by default).
    std::uint32_t score = 100;
    /// @brief Seconds between two shots (`fire_every`), or 0, the default, to never fire.
    float fire_every = 0.0F;
    /// @brief Seconds before its first shot (`first_shot`, 1 by default).
    float first_shot = 1.0F;
};

/// @brief Directory of the enemy scripts the server plays.
/// @return `core::asset_path("enemies")`: `assets/enemies` of the repository, copied next to
/// `r-type_server` by the build.
[[nodiscard]] std::filesystem::path default_enemies_directory();

/// @brief The enemy types of a directory, as waves files name them.
/// @param directory Directory of the scripts.
/// @return The names of its `.lua` files without their extension, sorted; none if the
/// directory cannot be read.
[[nodiscard]] std::vector<std::string>
enemy_types(const std::filesystem::path& directory = default_enemies_directory());

/// @brief The enemy types of a directory of Lua scripts, and the behaviors of their enemies.
/// @details Each `<type>.lua` file describes one type with `enemy { ... }`: its values (see
/// EnemyType) and two optional functions, `spawn(self)` when an enemy appears and
/// `update(self, dt)` at each tick. `self` is the enemy's own table: `entity` (its id for the
/// `ecs` functions), `x` and `y` where it appeared, `random`, a number from 0 to 1 the game
/// draws for it, and `age`, its seconds alive; the script may keep its own fields there.
/// The scripts share one sandboxed state, with `ecs` (script::bind_components()) and
/// `events`. A behavior that fails, by an error or by taking the whole budget of the state,
/// is logged once and stopped: an `update` stops moving the enemies of its type, which keep
/// their speed, and the enemies of a type whose `spawn` failed are not created. Not
/// thread-safe, as the registry.
class EnemyScripts {
public:
    /// @brief Loads every `.lua` file of a directory.
    /// @param registry World the enemies live in, kept by reference.
    /// @param directory Directory of the scripts.
    /// @throws aegis2d::core::ConfigError If a file name is not a type name (lowercase
    /// letters, digits and `_`), a script fails, does not call `enemy { ... }` or gives a
    /// wrong value; the message names the file, and the line of a script error.
    explicit EnemyScripts(aegis2d::ecs::Registry& registry,
                          const std::filesystem::path& directory = default_enemies_directory());

    /// @brief The types loaded.
    /// @return One per script, sorted by name.
    [[nodiscard]] const std::vector<EnemyType>& types() const noexcept;

    /// @brief Finds a type by name.
    /// @param name Name of the type.
    /// @return The type, or `nullptr` if no script gives it.
    [[nodiscard]] const EnemyType* find(std::string_view name) const noexcept;

    /// @brief Creates an enemy and runs the `spawn` of its type.
    /// @details The enemy flies nowhere until its script moves it: it has a Transform, a
    /// Velocity of zero, a Sprite and a Collider of its type's size on the enemy layer, its
    /// Health, a ScoreValue, the ramming Damage, an OffscreenDespawn, a NetworkId and, for a
    /// type that fires, a `shmup::Weapon` with its `shmup::AutoTrigger`.
    /// @param type Name of the type.
    /// @param position Where it appears.
    /// @param random Number from 0 to 1 the script gets as `self.random`.
    /// @param shot_delay Seconds added to the type's first_shot.
    /// @return The enemy, or a null handle if the type's `spawn` failed, now or before (the
    /// enemy is destroyed: it would stand still).
    /// @throws std::invalid_argument If no script gives @p type.
    aegis2d::ecs::Entity spawn(std::string_view type, aegis2d::Vec2 position, float random = 0.0F,
                               float shot_delay = 0.0F);

    /// @brief Stops running the behavior of an enemy: it keeps its components and its speed.
    /// @param enemy Enemy created by spawn().
    void stop(aegis2d::ecs::Entity enemy);

    /// @brief Runs the `update` of every enemy alive, and forgets the others.
    /// @param seconds Duration of the tick.
    void update(float seconds);

    /// @brief State of the scripts.
    /// @return The state.
    [[nodiscard]] aegis2d::script::ScriptState& lua() noexcept;

private:
    aegis2d::ecs::Registry* registry_;
    aegis2d::script::ScriptState lua_;
    std::vector<EnemyType> types_;
};

} // namespace rtype
