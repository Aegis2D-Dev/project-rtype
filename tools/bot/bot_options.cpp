#include <rtype/bot_options.hpp>
#include <rtype/client_options.hpp>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace rtype {

namespace {

// Decimal digits only, no sign nor space, that fit in 32 bits.
std::optional<std::uint32_t> parse_number(std::string_view text) noexcept {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string_view::npos) {
        return std::nullopt;
    }
    std::uint32_t number = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return number;
}

} // namespace

std::variant<BotOptions, std::string> parse_bot_options(std::span<const std::string_view> args) {
    BotOptions options;
    for (auto next = args.begin(); next != args.end(); ++next) {
        const std::string_view arg = *next;
        if (arg == "--help") {
            options.command = BotCommand::Help;
            return options;
        }
        if (arg == "--version") {
            options.command = BotCommand::Version;
            return options;
        }
        if (arg != "--host" && arg != "--port" && arg != "--name" && arg != "--seconds" &&
            arg != "--seed" && arg != "--count" && arg != "--inputs") {
            return std::format("unknown argument \"{}\"", arg);
        }
        if (++next == args.end()) {
            return std::format("{} needs a value", arg);
        }
        const std::string_view value = *next;
        if (arg == "--host") {
            options.settings.server.host = value;
        } else if (arg == "--name") {
            options.settings.server.name = value;
        } else if (arg == "--port") {
            const std::optional<std::uint16_t> port = parse_port(value);
            if (!port.has_value()) {
                return std::format("--port: \"{}\" is not a port (1 to 65535)", value);
            }
            options.settings.server.tcp_port = *port;
        } else if (arg == "--inputs") {
            options.inputs = std::filesystem::path{value};
        } else if (arg == "--count") {
            const std::optional<std::uint32_t> count = parse_number(value);
            if (!count.has_value() || *count == 0 || *count > max_bots) {
                return std::format("--count: \"{}\" is not a number of bots (1 to {})", value,
                                   max_bots);
            }
            options.count = *count;
        } else if (arg == "--seconds") {
            const std::optional<std::uint32_t> seconds = parse_number(value);
            if (!seconds.has_value() || *seconds == 0) {
                return std::format("--seconds: \"{}\" is not a number of seconds", value);
            }
            options.seconds = seconds;
        } else {
            const std::optional<std::uint32_t> seed = parse_number(value);
            if (!seed.has_value()) {
                return std::format("--seed: \"{}\" is not a 32-bit number", value);
            }
            options.settings.seed = *seed;
            options.seeded = true;
        }
    }
    return options;
}

} // namespace rtype
