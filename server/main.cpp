#include <aegis2d/core/config.hpp>
#include <aegis2d/core/module.hpp>
#include <aegis2d/ecs/module.hpp>
#include <aegis2d/ecs/waves.hpp>
#include <aegis2d/net/module.hpp>

#include <rtype/server_game.hpp>
#include <rtype/waves.hpp>

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>

namespace {
constexpr std::string_view version = RTYPE_VERSION;
constexpr std::string_view usage = "usage: r-type_server [--version] [--waves <file.json>]\n";
} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    std::filesystem::path waves_file = rtype::default_waves_file();
    bool waves_next = false;
    for (const std::string_view arg : args.subspan(1)) {
        if (waves_next) {
            waves_file = arg;
            waves_next = false;
        } else if (arg == "--version") {
            std::cout << "r-type_server " << version << '\n';
            return 0;
        } else if (arg == "--waves") {
            waves_next = true;
        } else {
            std::cerr << "r-type_server: unknown argument \"" << arg << "\"\n" << usage;
            return 2;
        }
    }
    if (waves_next) {
        std::cerr << "r-type_server: --waves needs a file\n" << usage;
        return 2;
    }
    std::cout << "r-type_server " << version << " (engine modules: " << aegis2d::core::name()
              << ", " << aegis2d::ecs::name() << ", " << aegis2d::net::name() << ")\n";

    rtype::ServerGame game;
    try {
        game.play_waves(rtype::load_waves(waves_file));
    } catch (const aegis2d::core::ConfigError& error) {
        std::cerr << "r-type_server: " << error.what() << '\n';
        return 1;
    }
    const aegis2d::ecs::WaveSchedule& waves = game.waves();
    std::cout << "waves: " << waves_file.string() << " (" << waves.waves()
              << (waves.waves() == 1 ? " wave, " : " waves, ") << waves.spawns().size()
              << " enemies in " << waves.duration() << " s" << (waves.loops() ? ", looping" : "")
              << ")\n";
    std::cout << "systems:";
    for (const std::string_view system : game.systems().names()) {
        std::cout << ' ' << system;
    }
    std::cout << '\n';
    return 0;
}
