#include "typed_character.hpp"

#include <aegis2d/platform/keyboard.hpp>

#include <optional>

namespace rtype {

using aegis2d::platform::Key;

std::optional<char> typed_character(Key key, bool shift) noexcept {
    const auto value = static_cast<int>(key);
    if (key >= Key::A && key <= Key::Z) {
        const int letter = value - static_cast<int>(Key::A);
        return static_cast<char>((shift ? 'A' : 'a') + letter);
    }
    if (key >= Key::Num0 && key <= Key::Num9) {
        return static_cast<char>('0' + (value - static_cast<int>(Key::Num0)));
    }
    if (key >= Key::Keypad0 && key <= Key::Keypad9) {
        return static_cast<char>('0' + (value - static_cast<int>(Key::Keypad0)));
    }
    switch (key) {
    case Key::Space:
        return ' ';
    case Key::Period:
    case Key::KeypadPeriod:
        return '.';
    case Key::Minus:
        return shift ? '_' : '-';
    case Key::KeypadMinus:
        return '-';
    case Key::Semicolon:
        return shift ? std::optional<char>{':'} : std::nullopt;
    case Key::LeftBracket:
        return '[';
    case Key::RightBracket:
        return ']';
    default:
        return std::nullopt;
    }
}

} // namespace rtype
