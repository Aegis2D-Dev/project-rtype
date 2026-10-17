#include "preferences.hpp"

#include <aegis2d/core/config.hpp>
#include <aegis2d/core/log.hpp>
#include <aegis2d/platform/settings.hpp>

#include <exception>
#include <filesystem>

namespace rtype {

namespace platform = aegis2d::platform;

namespace {

constexpr auto input_section = "input";
constexpr auto auto_fire_path = "accessibility.auto_fire";
constexpr auto gamepad_section = "gamepad";
constexpr auto effects_section = "accessibility";
constexpr auto text_size_path = "accessibility.text_size";

} // namespace

Preferences load_preferences(const std::filesystem::path& file) {
    Preferences preferences;
    preferences.file = file;
    const aegis2d::core::Config settings = platform::read_settings(file);
    preferences.controls.load(settings, input_section, auto_fire_path, gamepad_section);
    preferences.effects.load(settings, effects_section);
    preferences.text_size.load(settings, text_size_path);
    return preferences;
}

void save_preferences(const Preferences& preferences) {
    try {
        aegis2d::core::Config settings = platform::read_settings(preferences.file);
        preferences.controls.save(settings, input_section, auto_fire_path, gamepad_section);
        preferences.effects.save(settings, effects_section);
        preferences.text_size.save(settings, text_size_path);
        settings.save(preferences.file);
    } catch (const std::exception& error) {
        aegis2d::core::log_warn("settings not saved to {}: {}", preferences.file.string(),
                                error.what());
    }
}

std::filesystem::path default_preferences_file() {
    return platform::settings_dir("Aegis2D", "RType") / "settings.json";
}

} // namespace rtype
