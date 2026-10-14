#include <aegis2d/core/paths.hpp>
#include <aegis2d/ecs/waves.hpp>

#include <rtype/waves.hpp>

#include <filesystem>

namespace rtype {

std::filesystem::path default_waves_file() {
    return aegis2d::core::asset_path("waves/stage1.json");
}

aegis2d::ecs::WaveSchedule load_waves(const std::filesystem::path& path) {
    return aegis2d::ecs::WaveSchedule::from_file(path, enemy_types);
}

} // namespace rtype
