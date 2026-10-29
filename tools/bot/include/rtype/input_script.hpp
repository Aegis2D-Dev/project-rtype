/// @file
/// @brief Recorded inputs for `r-type_bot --inputs`: what to hold, and for how long.
#pragma once
#include <aegis2d/ecs/components.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rtype {

/// @brief One line of an input script: actions held for a time.
struct ScriptStep {
    /// @brief How long the actions are held.
    std::chrono::milliseconds duration{};
    /// @brief Held actions, one ecs::Action bit each.
    std::uint8_t held = 0;
};

/// @brief A sequence of inputs, played in order, then again from the start.
struct InputScript {
    /// @brief The steps, at least one, each at least 1 ms long.
    std::vector<ScriptStep> steps;
};

/// @brief Reads an input script.
/// @details One step per line: a duration in milliseconds, then the actions held, as the
/// letters `U` (up), `D` (down), `L` (left), `R` (right) and `F` (fire), or `-` for none:
/// @code
/// # Dodge up and down while firing, then rest.
/// 500 UF
/// 500 DF
/// 250 -
/// @endcode
/// Blank lines and lines starting with `#` are skipped; spaces and tabs separate the fields.
/// @param text Contents of the script.
/// @return The script, or a message naming the faulty line.
[[nodiscard]] std::variant<InputScript, std::string> parse_input_script(std::string_view text);

/// @brief Reads an input script from a file.
/// @param file Script file.
/// @return The script, or a message naming the file and the faulty line.
[[nodiscard]] std::variant<InputScript, std::string>
load_input_script(const std::filesystem::path& file);

/// @brief Plays an input script from the first call to update(), looping at its end.
class ScriptedInputs {
public:
    /// @brief Clock of the steps.
    using Clock = std::chrono::steady_clock;

    /// @brief Creates the player of a script.
    /// @param script Script to play; one without steps holds nothing.
    explicit ScriptedInputs(InputScript script);

    /// @brief The input to hold now, by the time since the first call.
    /// @param now Current time.
    /// @return The held actions.
    [[nodiscard]] aegis2d::ecs::PlayerInput update(Clock::time_point now);

    /// @brief Number of steps started.
    /// @return The count since the first call, the repeats included.
    [[nodiscard]] std::uint64_t picks() const noexcept;

private:
    InputScript script_;
    std::size_t step_ = 0;
    Clock::time_point until_{};
    std::uint64_t picks_ = 0;
};

} // namespace rtype
