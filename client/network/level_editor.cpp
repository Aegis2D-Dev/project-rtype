#include <aegis2d/core/config.hpp>
#include <aegis2d/core/math.hpp>

#include <rtype/level_editor.hpp>
#include <rtype/tuning.hpp>
#include <rtype/waves.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace rtype {

namespace core = aegis2d::core;
using aegis2d::Vec2;

namespace {

float number_or(const core::Config& config, const std::string& path, float fallback) {
    return static_cast<float>(config.number(path).value_or(fallback));
}

Vec2 vec2_or(const core::Config& config, const std::string& path, Vec2 fallback) {
    return Vec2{number_or(config, path + ".0", fallback.x),
                number_or(config, path + ".1", fallback.y)};
}

// Within the play area and the strip beyond its right edge, in whole pixels.
Vec2 kept(Vec2 position) noexcept {
    return Vec2{std::round(std::clamp(position.x, 0.0F, tuning::area_width + EditorView::strip)),
                std::round(std::clamp(position.y, 0.0F, tuning::area_height))};
}

} // namespace

LevelDraft LevelDraft::from_file(const std::filesystem::path& path) {
    // Checked as the server reads it; then read again as written, group by group.
    static_cast<void>(load_waves(path));
    const core::Config config = core::Config::from_file(path);
    LevelDraft draft{.loop = config.boolean("loop").value_or(false), .waves = {}};
    for (std::size_t wave = 0; config.contains(std::format("waves.{}", wave)); ++wave) {
        const std::string at_wave = std::format("waves.{}", wave);
        DraftWave& drafted = draft.waves.emplace_back(
            DraftWave{.delay = number_or(config, at_wave + ".delay", 0.0F), .enemies = {}});
        for (std::size_t group = 0; config.contains(std::format("{}.spawns.{}", at_wave, group));
             ++group) {
            const std::string at_group = std::format("{}.spawns.{}", at_wave, group);
            const std::string type = config.string(at_group + ".type").value_or("");
            const Vec2 position = vec2_or(config, at_group + ".position", Vec2{});
            const Vec2 step = vec2_or(config, at_group + ".step", Vec2{});
            const float at = number_or(config, at_group + ".at", 0.0F);
            const float every = number_or(config, at_group + ".every", 0.0F);
            const std::int64_t count = config.integer(at_group + ".count").value_or(1);
            for (std::int64_t member = 0; member < count; ++member) {
                const auto index = static_cast<float>(member);
                drafted.enemies.push_back({.type = type,
                                           .position = position + (step * index),
                                           .at = at + (every * index)});
            }
        }
    }
    return draft;
}

std::string LevelDraft::to_json(std::size_t first_wave) const {
    std::string json =
        std::format("{{\n    \"loop\": {},\n    \"waves\": [", loop ? "true" : "false");
    for (std::size_t wave = first_wave; wave < waves.size(); ++wave) {
        const DraftWave& written = waves.at(wave);
        json += std::format("{}\n        {{\n            \"delay\": {},\n            \"spawns\": [",
                            wave == first_wave ? "" : ",", written.delay);
        for (std::size_t enemy = 0; enemy < written.enemies.size(); ++enemy) {
            const DraftEnemy& spawn = written.enemies.at(enemy);
            std::string escaped;
            for (const char character : spawn.type) {
                if (character == '"' || character == '\\') {
                    escaped += '\\';
                }
                escaped += character;
            }
            json +=
                std::format("{}\n                {{\"type\": \"{}\", \"position\": [{}, {}]{}}}",
                            enemy == 0 ? "" : ",", escaped, spawn.position.x, spawn.position.y,
                            spawn.at != 0.0F ? std::format(", \"at\": {}", spawn.at) : "");
        }
        json += "\n            ]\n        }";
    }
    json += "\n    ]\n}\n";
    return json;
}

void LevelDraft::save(const std::filesystem::path& path) const {
    std::filesystem::path written = path;
    written += ".saving";
    {
        std::ofstream out{written, std::ios::binary | std::ios::trunc};
        out << to_json();
        if (!out.flush()) {
            throw core::ConfigError{std::format("{}: cannot be written", written.string())};
        }
    }
    std::error_code error;
    try {
        static_cast<void>(load_waves(written));
    } catch (const core::ConfigError&) {
        std::filesystem::remove(written, error);
        throw;
    }
    std::filesystem::rename(written, path, error);
    if (error) {
        std::filesystem::remove(written, error);
        throw core::ConfigError{std::format("{}: cannot be written", path.string())};
    }
}

aegis2d::ecs::WaveSchedule LevelDraft::schedule(std::size_t first_wave) const {
    return aegis2d::ecs::WaveSchedule::from_string(to_json(first_wave));
}

Vec2 EditorView::to_screen(Vec2 level) const noexcept {
    return origin + (level * scale);
}

Vec2 EditorView::to_level(Vec2 screen) const noexcept {
    return (screen - origin) / scale;
}

float EditorView::timeline_seconds(float end) noexcept {
    return std::max(10.0F, end * 1.2F);
}

bool EditorView::on_timeline(Vec2 screen) const noexcept {
    return screen.x >= timeline.x && screen.x <= timeline.x + timeline_size.x &&
           screen.y >= timeline.y && screen.y <= timeline.y + timeline_size.y;
}

LevelEditor::LevelEditor(LevelDraft draft, std::string type)
    : draft_{std::move(draft)}, type_{std::move(type)} {
    if (draft_.waves.empty()) {
        draft_.waves.emplace_back();
    }
}

const LevelDraft& LevelEditor::draft() const noexcept {
    return draft_;
}

std::size_t LevelEditor::wave() const noexcept {
    return wave_;
}

void LevelEditor::next_wave() noexcept {
    wave_ = (wave_ + 1) % draft_.waves.size();
    hovered_.reset();
    dragged_.reset();
}

void LevelEditor::previous_wave() noexcept {
    wave_ = (wave_ + draft_.waves.size() - 1) % draft_.waves.size();
    hovered_.reset();
    dragged_.reset();
}

std::vector<WaveSpan> LevelEditor::timeline() const {
    std::vector<WaveSpan> spans;
    float last = 0.0F;
    for (const DraftWave& wave : draft_.waves) {
        const float start = last + wave.delay;
        float end = start;
        for (const DraftEnemy& enemy : wave.enemies) {
            end = std::max(end, start + enemy.at);
        }
        spans.push_back({.start = start, .end = end});
        last = end;
    }
    return spans;
}

void LevelEditor::add_wave() {
    const auto after = draft_.waves.begin() + static_cast<std::ptrdiff_t>(wave_) + 1;
    draft_.waves.insert(after, DraftWave{.delay = 2.0F, .enemies = {}});
    ++wave_;
    hovered_.reset();
    dragged_.reset();
    changed_ = true;
}

void LevelEditor::shift_delay(float seconds) {
    float& delay = draft_.waves.at(wave_).delay;
    const float shifted = std::max(0.0F, std::round((delay + seconds) * 10.0F) / 10.0F);
    changed_ = changed_ || shifted != delay;
    delay = shifted;
}

void LevelEditor::move_wave(bool later) {
    const std::size_t other = later ? wave_ + 1 : wave_ - 1;
    if ((later && other >= draft_.waves.size()) || (!later && wave_ == 0)) {
        return;
    }
    std::swap(draft_.waves.at(wave_), draft_.waves.at(other));
    wave_ = other;
    hovered_.reset();
    dragged_.reset();
    changed_ = true;
}

bool LevelEditor::dragging_wave() const noexcept {
    return timeline_grab_.has_value();
}

std::span<const DraftEnemy> LevelEditor::enemies() const noexcept {
    return draft_.waves.at(wave_).enemies;
}

std::vector<DraftEnemy>& LevelEditor::wave_enemies() {
    return draft_.waves.at(wave_).enemies;
}

std::optional<std::size_t> LevelEditor::enemy_at(Vec2 level) const {
    // The one drawn last, on top, first.
    const std::span<const DraftEnemy> shown = enemies();
    const auto under =
        std::ranges::find_if(shown | std::views::reverse, [level](const DraftEnemy& enemy) {
            const Vec2 offset = enemy.position - level;
            return std::hypot(offset.x, offset.y) <= pick_radius;
        });
    if (under == (shown | std::views::reverse).end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(under, (shown | std::views::reverse).end())) - 1;
}

void LevelEditor::update(const EditorPointer& pointer, const EditorView& view) {
    // A wave taken on the timeline follows the mouse until the button is up.
    if (timeline_grab_.has_value()) {
        if (pointer.held) {
            float& delay = draft_.waves.at(wave_).delay;
            const float moved =
                grabbed_delay_ + ((pointer.position.x - *timeline_grab_) * seconds_per_pixel_);
            const float kept_delay = std::max(0.0F, std::round(moved * 10.0F) / 10.0F);
            changed_ = changed_ || kept_delay != delay;
            delay = kept_delay;
            return;
        }
        timeline_grab_.reset();
    }
    if (view.on_timeline(pointer.position)) {
        hovered_.reset();
        if (pointer.pressed) {
            // The wave started last before the time clicked, the first one before them all.
            const std::vector<WaveSpan> spans = timeline();
            seconds_per_pixel_ =
                EditorView::timeline_seconds(spans.back().end) / view.timeline_size.x;
            const float time = (pointer.position.x - view.timeline.x) * seconds_per_pixel_;
            wave_ = 0;
            for (std::size_t index = 0; index < spans.size(); ++index) {
                if (spans.at(index).start <= time) {
                    wave_ = index;
                }
            }
            dragged_.reset();
            timeline_grab_ = pointer.position.x;
            grabbed_delay_ = draft_.waves.at(wave_).delay;
        }
        return;
    }
    const Vec2 level = view.to_level(pointer.position);
    std::vector<DraftEnemy>& placed = wave_enemies();
    if (pointer.remove) {
        if (const std::optional<std::size_t> under = enemy_at(level)) {
            placed.erase(placed.begin() + static_cast<std::ptrdiff_t>(*under));
            dragged_.reset();
            changed_ = true;
        }
    }
    if (pointer.pressed) {
        dragged_ = enemy_at(level);
        if (!dragged_.has_value()) {
            placed.push_back({.type = type_, .position = kept(level), .at = 0.0F});
            dragged_ = placed.size() - 1;
            changed_ = true;
        }
        grab_ = placed.at(*dragged_).position - level;
    }
    if (dragged_.has_value() && pointer.held) {
        DraftEnemy& moved = placed.at(*dragged_);
        const Vec2 position = kept(level + grab_);
        changed_ = changed_ || position != moved.position;
        moved.position = position;
    } else {
        dragged_.reset();
    }
    hovered_ = dragged_.has_value() ? dragged_ : enemy_at(level);
}

std::optional<std::size_t> LevelEditor::hovered() const noexcept {
    return hovered_;
}

std::optional<std::size_t> LevelEditor::dragged() const noexcept {
    return dragged_;
}

bool LevelEditor::changed() const noexcept {
    return changed_;
}

void LevelEditor::mark_saved() noexcept {
    changed_ = false;
}

} // namespace rtype
