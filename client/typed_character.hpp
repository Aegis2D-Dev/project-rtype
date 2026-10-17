/// @file
/// @brief The character a key types, for the fields of the connect screen.
#pragma once
#include <aegis2d/platform/keyboard.hpp>

#include <optional>

namespace rtype {

/// @brief The character typed by a key, as on a US keyboard.
/// @details Letters follow the player's layout (the engine reads the key's symbol first):
/// lower case, upper case with @p shift. Digits come from the top row (whatever the layout
/// needs to reach them) and the keypad. The rest: Space, `.` (Period, keypad), `-` and `_`
/// (Minus), `:` (Shift and Semicolon), `[` and `]`.
/// @param key Key pressed.
/// @param shift Whether a Shift key is held.
/// @return The character, or nothing for a key that types none.
[[nodiscard]] std::optional<char> typed_character(aegis2d::platform::Key key, bool shift) noexcept;

} // namespace rtype
