#include "game_art.hpp"

#include <aegis2d/core/math.hpp>
#include <aegis2d/core/paths.hpp>
#include <aegis2d/render/color.hpp>
#include <aegis2d/render/color_vision.hpp>
#include <aegis2d/render/image.hpp>
#include <aegis2d/render/player_style.hpp>
#include <aegis2d/render/renderer.hpp>
#include <aegis2d/render/sprite_sheet.hpp>
#include <aegis2d/render/texture_manager.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace rtype {

namespace render = aegis2d::render;
using aegis2d::Vec2;

namespace {

constexpr int marker_pixels = 48;

// File and frame size of each sheet of Art, in its order.
constexpr std::array<std::string_view, art_count> art_files{"ship", "bydos", "explosion"};
constexpr std::array<render::TextureSize, art_count> art_frames{{
    {.width = 16, .height = 9},
    {.width = 12, .height = 10},
    {.width = 16, .height = 16},
}};
constexpr std::array<std::uint32_t, art_count> art_counts{4, 2, 6};

std::size_t index_of(Art art) noexcept {
    return static_cast<std::size_t>(art);
}

// The image as a vision sees it, pixel by pixel: the colors of the sprite change too, not
// only its tint.
render::Image seen_image(const render::Image& image, render::ColorVision vision) {
    std::vector<std::uint8_t> pixels = image.pixels;
    const auto byte = [](float value) {
        return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    };
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const render::Color color = render::simulate(image.pixel(x, y), vision);
            const std::size_t at =
                ((static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width)) +
                 static_cast<std::size_t>(x)) *
                4;
            pixels.at(at) = byte(color.r);
            pixels.at(at + 1) = byte(color.g);
            pixels.at(at + 2) = byte(color.b);
        }
    }
    return render::Image::from_pixels(image.width, image.height, std::move(pixels));
}

} // namespace

GameArt::GameArt(render::TextureManager& textures) : textures_{textures} {
    for (std::size_t art = 0; art < art_count; ++art) {
        const std::string_view file = art_files.at(art);
        const render::Image image =
            render::Image::load(aegis2d::core::asset_path(std::format("sprites/{}.png", file)));
        for (std::size_t vision = 0; vision < vision_count; ++vision) {
            const render::TextureId texture =
                textures.add(std::format("{}.{}", file, vision_names.at(vision)),
                             seen_image(image, render::color_visions.at(vision)));
            textures.set_sheet(
                texture, render::SpriteSheet::grid({.width = image.width, .height = image.height},
                                                   art_frames.at(art), art_counts.at(art)));
            sheets_.at(art).at(vision) = texture;
        }
    }
    for (std::size_t player = 0; player < render::max_players; ++player) {
        markers_.at(player) = textures.add(
            std::format("marker.{}", player),
            render::Image::from_pixels(
                marker_pixels, marker_pixels,
                render::marker_image(render::player_style(player).marker, marker_pixels)));
    }
}

render::TextureId GameArt::texture(Art art, std::size_t vision) const {
    return sheets_.at(index_of(art)).at(vision % vision_count);
}

std::uint32_t GameArt::frames(Art art) const noexcept {
    return art_counts.at(index_of(art));
}

Vec2 GameArt::frame_size(Art art) const {
    const render::TextureSize frame = art_frames.at(index_of(art));
    return {static_cast<float>(frame.width), static_cast<float>(frame.height)};
}

render::Rect GameArt::source(Art art, std::uint32_t frame) const {
    return textures_.get().source(texture(art, 0), frame % frames(art));
}

render::TextureId GameArt::marker(std::size_t player) const {
    return markers_.at(player % markers_.size());
}

render::Color seen_as(render::Color color, std::size_t vision) {
    return render::simulate(color, render::color_visions.at(vision % vision_count));
}

} // namespace rtype
