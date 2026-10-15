#include <rtype/server_options.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace rtype {

namespace {

// Decimal digits only: no sign, space or base prefix, and at most 65535.
std::optional<std::uint16_t> parse_port(std::string_view text) noexcept {
    constexpr std::uint32_t max_port = 65535;
    if (text.empty() || text.size() > 5) {
        return std::nullopt;
    }
    std::uint32_t port = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') {
            return std::nullopt;
        }
        port = (port * 10U) + static_cast<std::uint32_t>(digit - '0');
    }
    if (port > max_port) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(port);
}

} // namespace

std::expected<ServerOptions, std::string>
parse_server_options(std::span<const std::string_view> args) {
    ServerOptions options;
    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string_view arg = args.at(index);
        if (arg == "--help") {
            options.command = ServerCommand::Help;
            return options;
        }
        if (arg == "--version") {
            options.command = ServerCommand::Version;
            return options;
        }
        if (arg != "--tcp-port" && arg != "--udp-port" && arg != "--waves") {
            return std::unexpected{std::format("unknown argument \"{}\"", arg)};
        }
        if (index + 1 == args.size()) {
            return std::unexpected{std::format("{} needs a value", arg)};
        }
        const std::string_view value = args.at(++index);
        if (arg == "--waves") {
            options.waves = value;
            continue;
        }
        const std::optional<std::uint16_t> port = parse_port(value);
        if (!port) {
            return std::unexpected{
                std::format("{}: \"{}\" is not a port (0 to 65535)", arg, value)};
        }
        (arg == "--tcp-port" ? options.tcp_port : options.udp_port) = *port;
    }
    return options;
}

} // namespace rtype
