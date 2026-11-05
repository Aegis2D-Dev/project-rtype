/// @file
/// @brief Enemy waves of R-Type: the types a waves file can name and where the files are.
#pragma once
#include <aegis2d/ecs/waves.hpp>

#include <array>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

/// @brief Enemy types a waves file can name, each created by spawn_enemy().
/// @details Add a name here and its branch in spawn_enemy() to use a new enemy in waves
/// (see the "Add an enemy type" guide); the size of the array follows.
inline constexpr std::array enemy_types{std::string_view{"bydos_slave"}};

/// @brief Waves file played by the server when none is given.
/// @return `core::asset_path("waves/stage1.json")`: `assets/waves/stage1.json` of the
/// repository, copied next to `r-type_server` by the build.
[[nodiscard]] std::filesystem::path default_waves_file();

/// @brief The waves files of a directory, as a room names them (RFC section 3.6).
/// @param directory Directory to look in, such as the parent of default_waves_file().
/// @return The names of its `.json` files without their extension, sorted; none if the
/// directory cannot be read.
[[nodiscard]] std::vector<std::string> waves_names(const std::filesystem::path& directory);

/// @brief Reads a waves file, accepting only the types of enemy_types.
/// @param path JSON file in the format of `ecs::WaveSchedule`.
/// @return The schedule, ready for `ServerGame::play_waves()`.
/// @throws aegis2d::core::ConfigError If the file cannot be read or is not valid waves (an
/// unknown type included); the message names the file and the faulty value.
[[nodiscard]] aegis2d::ecs::WaveSchedule load_waves(const std::filesystem::path& path);

} // namespace rtype
