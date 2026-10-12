#include <aegis2d/core/module.hpp>
#include <aegis2d/ecs/module.hpp>
#include <aegis2d/net/module.hpp>

#include <rtype/server_game.hpp>

#include <iostream>
#include <span>
#include <string_view>

namespace {
constexpr std::string_view version = RTYPE_VERSION;
} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    for (const std::string_view arg : args.subspan(1)) {
        if (arg == "--version") {
            std::cout << "r-type_server " << version << '\n';
            return 0;
        }
    }
    std::cout << "r-type_server " << version << " (engine modules: " << aegis2d::core::name()
              << ", " << aegis2d::ecs::name() << ", " << aegis2d::net::name() << ")\n";
    const rtype::ServerGame game;
    std::cout << "systems:";
    for (const std::string_view system : game.systems().names()) {
        std::cout << ' ' << system;
    }
    std::cout << '\n';
    return 0;
}
