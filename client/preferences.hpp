/// @file
/// @brief The player's own settings: keys, auto-fire, text size, flashes and shake.
#pragma once
#include <aegis2d/render/screen_effects.hpp>
#include <aegis2d/render/text_layout.hpp>
#include <aegis2d/shmup/ship_controls.hpp>

#include <cstddef>
#include <filesystem>

namespace rtype {

/// @brief The player's settings, read from a JSON file and saved at each change.
/// @details The sections are the engine's (the key bindings, auto-fire, HUD and effects
/// examples write the same ones): `input` and `gamepad` for the keys and buttons,
/// `accessibility.auto_fire`, `accessibility.text_size`, and `accessibility.flashes` /
/// `accessibility.shake`. A missing or broken file gives the defaults.
struct Preferences {
    /// @brief Settings file, created at the first save.
    std::filesystem::path file;
    /// @brief The ship's keys and buttons, and auto-fire.
    aegis2d::shmup::ShipControls controls;
    /// @brief Whether explosions flash and shake the screen.
    aegis2d::render::EffectSettings effects;
    /// @brief Size of the texts, 100 % to 200 %.
    aegis2d::render::TextSize text_size;
    /// @brief Color vision shown (index in render::color_visions). Not saved: F3 shows the
    /// game as other players see it, for a moment.
    std::size_t vision = 0;
};

/// @brief Reads the settings.
/// @param file Settings file; a missing one gives the defaults.
/// @return The settings.
[[nodiscard]] Preferences load_preferences(const std::filesystem::path& file);

/// @brief Writes every setting into its file, keeping the other settings of the file.
/// @details A failure (a folder that cannot be written) is logged, not thrown: the game
/// goes on with the settings it has.
/// @param preferences Settings to write.
void save_preferences(const Preferences& preferences);

/// @brief The default settings file: `settings.json` in the player's settings folder.
/// @return Its path.
[[nodiscard]] std::filesystem::path default_preferences_file();

} // namespace rtype
