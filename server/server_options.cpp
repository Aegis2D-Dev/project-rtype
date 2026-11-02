#include <rtype/server_options.hpp>

#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace rtype {

namespace {

// Decimal digits only (no sign, space or base prefix), from min to max.
std::optional<std::uint32_t> parse_number(std::string_view text, std::uint32_t min,
                                          std::uint32_t max) noexcept {
    if (text.empty() || text.size() > 6) {
        return std::nullopt;
    }
    std::uint32_t number = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') {
            return std::nullopt;
        }
        number = (number * 10U) + static_cast<std::uint32_t>(digit - '0');
    }
    if (number < min || number > max) {
        return std::nullopt;
    }
    return number;
}

constexpr std::uint32_t max_port = 65535;
constexpr std::uint32_t max_packet_rate = 100000;
constexpr std::uint32_t max_payload = 1182;

} // namespace

std::variant<ServerOptions, std::string>
parse_server_options(std::span<const std::string_view> args) {
    ServerOptions options;
    for (auto next = args.begin(); next != args.end(); ++next) {
        const std::string_view arg = *next;
        if (arg == "--help") {
            options.command = ServerCommand::Help;
            return options;
        }
        if (arg == "--version") {
            options.command = ServerCommand::Version;
            return options;
        }
        if (arg != "--tcp-port" && arg != "--udp-port" && arg != "--waves" &&
            arg != "--max-packet-rate" && arg != "--compress") {
            return std::format("unknown argument \"{}\"", arg);
        }
        if (++next == args.end()) {
            return std::format("{} needs a value", arg);
        }
        const std::string_view value = *next;
        if (arg == "--waves") {
            options.waves = value;
            continue;
        }
        if (arg == "--max-packet-rate") {
            const std::optional<std::uint32_t> rate = parse_number(value, 1, max_packet_rate);
            if (!rate) {
                return std::format("{}: \"{}\" is not a rate (1 to {})", arg, value,
                                   max_packet_rate);
            }
            options.packet_rate = *rate;
            continue;
        }
        if (arg == "--compress") {
            const std::optional<std::uint32_t> size = parse_number(value, 1, max_payload);
            if (!size) {
                return std::format("{}: \"{}\" is not a payload size (1 to {})", arg, value,
                                   max_payload);
            }
            options.compress_from = *size;
            continue;
        }
        const std::optional<std::uint32_t> port = parse_number(value, 0, max_port);
        if (!port) {
            return std::format("{}: \"{}\" is not a port (0 to 65535)", arg, value);
        }
        (arg == "--tcp-port" ? options.tcp_port : options.udp_port) =
            static_cast<std::uint16_t>(*port);
    }
    return options;
}

} // namespace rtype
