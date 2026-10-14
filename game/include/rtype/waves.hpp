/// @file
/// @brief Enemy waves of R-Type: the types a waves file can name and where the files are.
#pragma once
#include <aegis2d/ecs/waves.hpp>

#include <array>
#include <filesystem>
#include <span>
#include <string_view>

namespace rtype {

/// @brief Enemy types a waves file can name, each created by spawn_enemy().
inline constexpr std::array<std::string_view, 1> enemy_types{"bydos_slave"};

/// @brief Waves file played by the server when none is given.
/// @return `core::asset_path("waves/stage1.json")`: `assets/waves/stage1.json` of the
/// repository, copied next to `r-type_server` by the build.
[[nodiscard]] std::filesystem::path default_waves_file();

/// @brief Reads a waves file, accepting only the types of enemy_types.
/// @param path JSON file in the format of `ecs::WaveSchedule`.
/// @return The schedule, ready for `ServerGame::play_waves()`.
/// @throws aegis2d::core::ConfigError If the file cannot be read or is not valid waves (an
/// unknown type included); the message names the file and the faulty value.
[[nodiscard]] aegis2d::ecs::WaveSchedule load_waves(const std::filesystem::path& path);

} // namespace rtype
