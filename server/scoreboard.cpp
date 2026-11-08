#include <aegis2d/core/config.hpp>
#include <aegis2d/core/log.hpp>

#include <rtype/login.hpp>
#include <rtype/scoreboard.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

namespace core = aegis2d::core;

Scoreboard::Scoreboard(std::filesystem::path file) : file_{std::move(file)} {
    load();
}

// A missing file is an empty board; anything else that is not a board is reported.
void Scoreboard::load() {
    std::error_code missing;
    if (!std::filesystem::exists(file_, missing)) {
        return;
    }
    try {
        const core::Config config = core::Config::from_file(file_);
        const std::optional<core::ConfigValue> scores = config.value("scores");
        const auto* const list =
            scores.has_value() ? std::get_if<core::ConfigValue::Array>(&scores->data) : nullptr;
        if (list == nullptr) {
            throw core::ConfigError{std::format("{}: no \"scores\" array", file_.string())};
        }
        std::vector<ScoreEntry> read;
        for (std::size_t index = 0; index < list->size(); ++index) {
            const std::string at = std::format("scores.{}", index);
            const std::optional<std::string> name = config.string(at + ".name");
            const std::optional<std::int64_t> score = config.integer(at + ".score");
            if (!name || !valid_player_name(*name) || !score ||
                !std::in_range<std::uint32_t>(*score)) {
                throw core::ConfigError{
                    std::format("{}: score {} is not a name and a number", file_.string(), index)};
            }
            read.push_back(ScoreEntry{.name = *name, .score = static_cast<std::uint32_t>(*score)});
        }
        // A hand-edited file may be in any order and too long.
        std::ranges::stable_sort(read, std::ranges::greater{}, &ScoreEntry::score);
        if (read.size() > max_entries) {
            read.resize(max_entries);
        }
        best_ = std::move(read);
    } catch (const core::ConfigError& error) {
        load_error_ = error.what();
        core::log_warn("scoreboard: {} (starting empty, the file is replaced at the next score)",
                       error.what());
    }
}

bool Scoreboard::record(std::string_view name, std::uint32_t score) {
    if (!valid_player_name(name)) {
        return false;
    }
    const auto place =
        std::ranges::upper_bound(best_, score, std::ranges::greater{}, &ScoreEntry::score);
    if (place == best_.end() && best_.size() >= max_entries) {
        return false;
    }
    best_.insert(place, ScoreEntry{.name = std::string{name}, .score = score});
    if (best_.size() > max_entries) {
        best_.pop_back();
    }
    save();
    return true;
}

void Scoreboard::save() const {
    core::ConfigValue::Array scores;
    scores.reserve(best_.size());
    for (const ScoreEntry& entry : best_) {
        scores.push_back(core::ConfigValue{core::ConfigValue::Object{
            {.key = "name", .value = {.data = entry.name}},
            {.key = "score", .value = {.data = static_cast<std::int64_t>(entry.score)}},
        }});
    }
    core::Config config;
    config.set("scores", core::ConfigValue{std::move(scores)});
    try {
        config.save(file_);
    } catch (const core::ConfigError& error) {
        core::log_error("scoreboard: {} (the scores stay in memory)", error.what());
    }
}

const std::vector<ScoreEntry>& Scoreboard::best() const noexcept {
    return best_;
}

const std::optional<std::string>& Scoreboard::load_error() const noexcept {
    return load_error_;
}

const std::filesystem::path& Scoreboard::file() const noexcept {
    return file_;
}

} // namespace rtype
