#include <aegis2d/core/paths.hpp>
#include <aegis2d/ecs/waves.hpp>

#include <rtype/waves.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace rtype {

std::vector<std::string> waves_names(const std::filesystem::path& directory) {
    std::vector<std::string> names;
    std::error_code error;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator{directory, error}) {
        if (entry.path().extension() == ".json") {
            names.push_back(entry.path().stem().string());
        }
    }
    std::ranges::sort(names);
    return names;
}

std::filesystem::path default_waves_file() {
    return aegis2d::core::asset_path("waves/stage1.json");
}

aegis2d::ecs::WaveSchedule load_waves(const std::filesystem::path& path,
                                      const std::vector<std::string>& types) {
    const std::vector<std::string_view> names{types.begin(), types.end()};
    return aegis2d::ecs::WaveSchedule::from_file(path, names);
}

} // namespace rtype
