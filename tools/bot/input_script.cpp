#include <aegis2d/ecs/components.hpp>

#include <rtype/input_script.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

namespace rtype {

namespace ecs = aegis2d::ecs;

namespace {

// Longest step: a minute, far above any hold of a player.
constexpr std::uint32_t longest_step_ms = 60'000;

std::string_view trim(std::string_view text) noexcept {
    const std::size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

std::optional<ecs::Action> action_of(char letter) noexcept {
    switch (letter) {
    case 'U':
        return ecs::Action::Up;
    case 'D':
        return ecs::Action::Down;
    case 'L':
        return ecs::Action::Left;
    case 'R':
        return ecs::Action::Right;
    case 'F':
        return ecs::Action::Fire;
    default:
        return std::nullopt;
    }
}

} // namespace

std::variant<InputScript, std::string> parse_input_script(std::string_view text) {
    InputScript script;
    std::size_t number = 0;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        const std::string_view line = trim(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        ++number;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::size_t gap = line.find_first_of(" \t");
        const std::string_view duration = line.substr(0, gap);
        const std::string_view actions =
            gap == std::string_view::npos ? std::string_view{} : trim(line.substr(gap));
        std::uint32_t milliseconds = 0;
        const auto [stop, error] =
            std::from_chars(duration.data(), duration.data() + duration.size(), milliseconds);
        if (error != std::errc{} || stop != duration.data() + duration.size() ||
            milliseconds == 0 || milliseconds > longest_step_ms) {
            return std::format("line {}: \"{}\" is not a duration (1 to {} ms)", number, duration,
                               longest_step_ms);
        }
        if (actions.empty()) {
            return std::format("line {}: no actions (U, D, L, R, F, or - for none)", number);
        }
        ecs::PlayerInput input;
        if (actions != "-") {
            for (const char letter : actions) {
                const std::optional<ecs::Action> action = action_of(letter);
                if (!action.has_value()) {
                    return std::format("line {}: \"{}\" is not an action (U, D, L, R, F or -)",
                                       number, letter);
                }
                input.set(*action, true);
            }
        }
        script.steps.push_back(
            ScriptStep{.duration = std::chrono::milliseconds{milliseconds}, .held = input.held});
    }
    if (script.steps.empty()) {
        return std::string{"no step: one line per step, \"<milliseconds> <actions>\""};
    }
    return script;
}

std::variant<InputScript, std::string> load_input_script(const std::filesystem::path& file) {
    std::ifstream stream{file, std::ios::binary};
    if (!stream) {
        return std::format("{}: cannot be read", file.string());
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    std::variant<InputScript, std::string> script = parse_input_script(contents.str());
    if (auto* const error = std::get_if<std::string>(&script)) {
        *error = std::format("{}: {}", file.string(), *error);
    }
    return script;
}

ScriptedInputs::ScriptedInputs(InputScript script) : script_{std::move(script)} {}

ecs::PlayerInput ScriptedInputs::update(Clock::time_point now) {
    if (script_.steps.empty()) {
        return {};
    }
    if (picks_ == 0) {
        until_ = now + script_.steps.front().duration;
        picks_ = 1;
    }
    // Steps that ended since the last call are skipped, in order, so that the script keeps
    // its timing whatever the calls.
    while (now >= until_) {
        step_ = (step_ + 1) % script_.steps.size();
        until_ += script_.steps.at(step_).duration;
        ++picks_;
    }
    return ecs::PlayerInput{.held = script_.steps.at(step_).held};
}

std::uint64_t ScriptedInputs::picks() const noexcept {
    return picks_;
}

} // namespace rtype
