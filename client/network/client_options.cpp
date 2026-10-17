#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>

#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace rtype {

namespace {

// Decimal digits only (no sign, space or base prefix), from 1 to 65535.
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
    if (port == 0 || port > max_port) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(port);
}

} // namespace

std::variant<ClientOptions, std::string>
parse_client_options(std::span<const std::string_view> args) {
    ClientOptions options;
    for (auto next = args.begin(); next != args.end(); ++next) {
        const std::string_view arg = *next;
        if (arg == "--help") {
            options.command = ClientCommand::Help;
            return options;
        }
        if (arg == "--version") {
            options.command = ClientCommand::Version;
            return options;
        }
        if (arg != "--host" && arg != "--port" && arg != "--name") {
            return std::format("unknown argument \"{}\"", arg);
        }
        if (++next == args.end()) {
            return std::format("{} needs a value", arg);
        }
        const std::string_view value = *next;
        if (arg == "--host") {
            options.settings.host = value;
        } else if (arg == "--name") {
            options.settings.name = value;
        } else {
            const std::optional<std::uint16_t> port = parse_port(value);
            if (!port) {
                return std::format("--port: \"{}\" is not a port (1 to 65535)", value);
            }
            options.settings.tcp_port = *port;
        }
    }
    return options;
}

} // namespace rtype
