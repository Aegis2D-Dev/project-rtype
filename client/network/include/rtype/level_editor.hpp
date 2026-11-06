/// @file
/// @brief The level editor's logic: the waves of a file as enemies to place with the mouse.
#pragma once
#include <aegis2d/core/math.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

/// @brief An enemy of a wave, where and when the editor places it.
struct DraftEnemy {
    /// @brief Its type, as a waves file names it.
    std::string type{};
    /// @brief Where it appears, in logical pixels of the play area.
    aegis2d::Vec2 position{};
    /// @brief Seconds after the start of its wave.
    float at = 0.0F;

    /// @brief Field-by-field equality.
    /// @return `true` if every field is equal.
    bool operator==(const DraftEnemy&) const = default;
};

/// @brief A wave of the editor.
struct DraftWave {
    /// @brief Seconds between the last enemy of the wave before and the start of this one.
    float delay = 0.0F;
    /// @brief Its enemies, in the order they were placed.
    std::vector<DraftEnemy> enemies{};
};

/// @brief A waves file opened in the editor.
/// @details Each group of the file (`count`, `every`, `step`) becomes one enemy per member,
/// so that each can be moved on its own; the waves they make are the same.
struct LevelDraft {
    /// @brief Whether the waves play again from the first after the last.
    bool loop = false;
    /// @brief The waves, in order.
    std::vector<DraftWave> waves{};

    /// @brief Reads a waves file, as the server does (load_waves()).
    /// @param path JSON file in the format of `ecs::WaveSchedule`.
    /// @return The draft.
    /// @throws aegis2d::core::ConfigError If the file cannot be read or is not valid waves.
    [[nodiscard]] static LevelDraft from_file(const std::filesystem::path& path);
};

/// @brief The mouse of a frame, in logical pixels of the screen.
struct EditorPointer {
    /// @brief Where it is.
    aegis2d::Vec2 position{};
    /// @brief The left button went down: place an enemy, or take the one under the mouse.
    bool pressed = false;
    /// @brief The left button is down: the enemy taken follows the mouse.
    bool held = false;
    /// @brief The right button went down: remove the enemy under the mouse.
    bool remove = false;
};

/// @brief Where the editor draws the play area: shrunk to show the strip beyond its right
/// edge, where the enemies appear, and the editor's text above.
struct EditorView {
    /// @brief Width of the strip beyond the right edge of the play area that is shown.
    static constexpr float strip = 96.0F;
    /// @brief Size of a logical pixel of the play area on screen.
    float scale = 0.85F;
    /// @brief Where the top-left corner of the play area is on screen.
    aegis2d::Vec2 origin{52.0F, 70.0F};

    /// @brief A point of the play area on screen.
    /// @param level Point of the play area.
    /// @return The point on screen.
    [[nodiscard]] aegis2d::Vec2 to_screen(aegis2d::Vec2 level) const noexcept;

    /// @brief The point of the play area under a point of the screen.
    /// @param screen Point on screen, such as the mouse.
    /// @return The point of the play area.
    [[nodiscard]] aegis2d::Vec2 to_level(aegis2d::Vec2 screen) const noexcept;
};

/// @brief Places and moves the enemies of a wave with the mouse.
/// @details The left button places an enemy of the current type where it is clicked, or takes
/// the enemy under the mouse; either follows the mouse while the button is held. The right
/// button removes the enemy under the mouse. Positions are whole logical pixels, kept within
/// the play area and the strip beyond its right edge (EditorView::strip).
class LevelEditor {
public:
    /// @brief Distance from an enemy's position within which the mouse is over it.
    static constexpr float pick_radius = 20.0F;

    /// @brief Edits a draft, from its first wave.
    /// @param draft Waves to edit; a draft without waves gets an empty one.
    /// @param type Type of the enemies placed.
    explicit LevelEditor(LevelDraft draft, std::string type = "bydos_slave");

    /// @brief The waves edited.
    /// @return The draft.
    [[nodiscard]] const LevelDraft& draft() const noexcept;

    /// @brief Index of the wave edited.
    /// @return From 0.
    [[nodiscard]] std::size_t wave() const noexcept;

    /// @brief Edits the next wave, or the first after the last.
    void next_wave() noexcept;

    /// @brief Edits the wave before, or the last before the first.
    void previous_wave() noexcept;

    /// @brief The enemies of the wave edited.
    /// @return Its enemies.
    [[nodiscard]] std::span<const DraftEnemy> enemies() const noexcept;

    /// @brief Applies the mouse of a frame.
    /// @param pointer The mouse, in logical pixels of the screen.
    /// @param view Where the play area is drawn.
    void update(const EditorPointer& pointer, const EditorView& view = {});

    /// @brief The enemy under the mouse, at the last update().
    /// @return Its index in enemies(), or nothing.
    [[nodiscard]] std::optional<std::size_t> hovered() const noexcept;

    /// @brief The enemy following the mouse.
    /// @return Its index in enemies(), or nothing while the left button is up.
    [[nodiscard]] std::optional<std::size_t> dragged() const noexcept;

    /// @brief Tells whether an enemy was placed, moved or removed since the editor opened.
    /// @return `true` once something changed.
    [[nodiscard]] bool changed() const noexcept;

private:
    [[nodiscard]] std::vector<DraftEnemy>& wave_enemies();
    [[nodiscard]] std::optional<std::size_t> enemy_at(aegis2d::Vec2 level) const;

    LevelDraft draft_;
    std::string type_;
    std::size_t wave_ = 0;
    std::optional<std::size_t> hovered_;
    std::optional<std::size_t> dragged_;
    aegis2d::Vec2 grab_{};
    bool changed_ = false;
};

} // namespace rtype
