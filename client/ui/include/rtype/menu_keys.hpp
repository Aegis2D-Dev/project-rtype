/// @file
/// @brief The keys of the lobby's screens, from the keyboard and from a gamepad.
#pragma once
#include <aegis2d/platform/gamepad.hpp>
#include <aegis2d/platform/keyboard.hpp>

#include <vector>

namespace rtype {

/// @brief The keys pressed during the frame, with the gamepad's menu buttons as the keys
/// they stand for.
/// @details The D-pad and the left stick are the arrows, South (A, Cross) is Enter and East
/// (B, Circle) is Escape, so the rooms, room, joining and connection lost screens take a
/// gamepad as they take the keyboard. The keyboard's keys come first, in the order they went
/// down; a button stands for its key only once, even when the key is pressed too.
/// @param keyboard Keyboard of the frame.
/// @param gamepad Gamepad of the frame.
/// @return The keys to apply, in that order.
[[nodiscard]] std::vector<aegis2d::platform::Key>
menu_keys(const aegis2d::platform::Keyboard& keyboard, const aegis2d::platform::Gamepad& gamepad);

} // namespace rtype
