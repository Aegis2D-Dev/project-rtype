#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace rtype {

namespace {

constexpr std::uint64_t default_screenshot_frames = 300;

std::optional<std::uint64_t> parse_count(std::string_view text) noexcept {
    constexpr std::size_t max_digits = 9;
    if (text.empty() || text.size() > max_digits) {
        return std::nullopt;
    }
    std::uint64_t count = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') {
            return std::nullopt;
        }
        count = (count * 10U) + static_cast<std::uint64_t>(digit - '0');
    }
    if (count == 0) {
        return std::nullopt;
    }
    return count;
}

} // namespace

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
        if (arg == "--join") {
            options.join = true;
            continue;
        }
        if (arg == "--metrics") {
            options.metrics = true;
            continue;
        }
        if (arg != "--host" && arg != "--port" && arg != "--name" && arg != "--settings" &&
            arg != "--frames" && arg != "--screenshot") {
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
        } else if (arg == "--settings") {
            options.settings_file = std::filesystem::path{value};
        } else if (arg == "--screenshot") {
            options.screenshot = std::filesystem::path{value};
        } else if (arg == "--frames") {
            const std::optional<std::uint64_t> frames = parse_count(value);
            if (!frames.has_value()) {
                return std::format("--frames: \"{}\" is not a number of frames", value);
            }
            options.frames = *frames;
        } else {
            const std::optional<std::uint16_t> port = parse_port(value);
            if (!port) {
                return std::format("--port: \"{}\" is not a port (1 to 65535)", value);
            }
            options.settings.tcp_port = *port;
        }
    }
    if (options.screenshot.has_value() && options.frames == 0) {
        options.frames = default_screenshot_frames;
    }
    return options;
}

} // namespace rtype
