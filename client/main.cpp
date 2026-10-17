#include <aegis2d/audio/module.hpp>
#include <aegis2d/core/module.hpp>
#include <aegis2d/ecs/module.hpp>
#include <aegis2d/net/module.hpp>
#include <aegis2d/render/module.hpp>

#include "client_app.hpp"
#include <rtype/client_options.hpp>

#include <cstddef>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
constexpr std::string_view version = RTYPE_VERSION;
} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::vector<std::string_view> arguments(args.begin() + 1, args.end());
    const std::variant<rtype::ClientOptions, std::string> parsed =
        rtype::parse_client_options(arguments);
    if (const std::string* const error = std::get_if<std::string>(&parsed)) {
        std::cerr << "r-type_client: " << *error << '\n' << rtype::client_usage;
        return 2;
    }
    // Not an error, so the options: get_if rather than get, which could throw.
    const auto& options = *std::get_if<rtype::ClientOptions>(&parsed);
    if (options.command == rtype::ClientCommand::Help) {
        std::cout << rtype::client_usage;
        return 0;
    }
    if (options.command == rtype::ClientCommand::Version) {
        std::cout << "r-type_client " << version << '\n';
        return 0;
    }
    std::cout << "r-type_client " << version << " (engine modules: " << aegis2d::core::name()
              << ", " << aegis2d::ecs::name() << ", " << aegis2d::net::name() << ", "
              << aegis2d::render::name() << ", " << aegis2d::audio::name() << ")\n";

    try {
        rtype::ClientApp app{options};
        app.run();
    } catch (const std::exception& error) {
        std::cerr << "r-type_client: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
