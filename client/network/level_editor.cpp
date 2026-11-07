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
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
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

Vec2 EditorView::to_screen(Vec2 level) const noexcept {
    return origin + (level * scale);
}

Vec2 EditorView::to_level(Vec2 screen) const noexcept {
    return (screen - origin) / scale;
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

} // namespace rtype
