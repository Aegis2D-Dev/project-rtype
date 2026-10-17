/// @file
/// @brief The client's sprite sheets and player markers, as each color vision sees them.
#pragma once
#include <aegis2d/core/math.hpp>
#include <aegis2d/render/color.hpp>
#include <aegis2d/render/color_vision.hpp>
#include <aegis2d/render/player_style.hpp>
#include <aegis2d/render/renderer.hpp>
#include <aegis2d/render/texture_manager.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace rtype {

/// @brief A sprite sheet of the game.
enum class Art : std::uint8_t {
    /// @brief The player's ship: 4 frames of 16 x 9, its engine flame flickering.
    Ship,
    /// @brief A Bydos: 2 frames of 12 x 10, moving its legs.
    Bydos,
    /// @brief An explosion: 6 frames of 16 x 16, a blast turning to smoke.
    Explosion,
};

/// @brief Number of sheets of Art.
inline constexpr std::size_t art_count = 3;

/// @brief Number of color visions the client can show (render::color_visions).
inline constexpr std::size_t vision_count = aegis2d::render::color_visions.size();

/// @brief Names of the color visions, in the order of render::color_visions.
inline constexpr std::array<std::string_view, vision_count> vision_names{
    "typical", "protanopia", "deuteranopia", "tritanopia", "achromatopsia"};

/// @brief The textures of the game, loaded once from `assets/sprites/`.
/// @details Each sheet is loaded once per color vision, its pixels changed as that vision
/// sees them, so that F3 shows the game as a player with a color vision deficiency sees it,
/// sprites included. The player markers (render::marker_image()) are white shapes, tinted
/// with the player's color when drawn.
class GameArt {
public:
    /// @brief Loads the sheets and makes the markers.
    /// @param textures Manager that keeps the textures; it must outlive this object.
    /// @throws render::ImageError if a sheet is missing or unreadable.
    explicit GameArt(aegis2d::render::TextureManager& textures);

    /// @brief Texture of a sheet as a vision sees it.
    /// @param art Sheet.
    /// @param vision Index in render::color_visions.
    /// @return The texture, cut into its frames.
    [[nodiscard]] aegis2d::render::TextureId texture(Art art, std::size_t vision) const;

    /// @brief Number of frames of a sheet.
    /// @param art Sheet.
    /// @return The count.
    [[nodiscard]] std::uint32_t frames(Art art) const noexcept;

    /// @brief Size of a frame of a sheet, in pixels of the art.
    /// @param art Sheet.
    /// @return Width and height.
    [[nodiscard]] aegis2d::Vec2 frame_size(Art art) const;

    /// @brief Part of a texture a frame draws.
    /// @param art Sheet.
    /// @param frame Frame, wrapped around the count.
    /// @return The source rectangle.
    [[nodiscard]] aegis2d::render::Rect source(Art art, std::uint32_t frame) const;

    /// @brief Marker texture of a player slot.
    /// @param player Player number, 0 to 3.
    /// @return The texture, white, to tint with the player's color.
    [[nodiscard]] aegis2d::render::TextureId marker(std::size_t player) const;

private:
    std::reference_wrapper<aegis2d::render::TextureManager> textures_;
    std::array<std::array<aegis2d::render::TextureId, vision_count>, art_count> sheets_{};
    std::array<aegis2d::render::TextureId, aegis2d::render::max_players> markers_{};
};

/// @brief A color as a vision sees it.
/// @param color Color to show.
/// @param vision Index in render::color_visions.
/// @return The color seen.
[[nodiscard]] aegis2d::render::Color seen_as(aegis2d::render::Color color, std::size_t vision);

} // namespace rtype
