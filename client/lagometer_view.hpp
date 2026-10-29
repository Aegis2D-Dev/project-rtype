/// @file
/// @brief The lagometer overlay: small graphs of the network statistics, toggled with F6.
#pragma once
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/renderer.hpp>

#include "preferences.hpp"
#include <rtype/lagometer.hpp>

namespace rtype {

/// @brief Draws the lagometer at the top right of the game screen.
/// @details One graph per LagGraph, with its last value: a bar per sample of the last 10
/// seconds, newest on the right, green, yellow or red by lag_level(); the bandwidth in blue.
/// @param renderer Renderer, between begin_frame() and end_frame().
/// @param font Font of the labels.
/// @param lagometer Samples to draw.
/// @param preferences The player's settings, for the color vision.
void draw_lagometer(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
                    const Lagometer& lagometer, const Preferences& preferences);

} // namespace rtype
