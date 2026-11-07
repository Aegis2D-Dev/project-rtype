#include <rtype/menu_keys.hpp>

#include <aegis2d/platform/gamepad.hpp>
#include <aegis2d/platform/keyboard.hpp>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace rtype {

namespace platform = aegis2d::platform;
using platform::GamepadButton;
using platform::Key;

std::vector<Key> menu_keys(const platform::Keyboard& keyboard, const platform::Gamepad& gamepad) {
    static constexpr std::array buttons{
        std::pair{GamepadButton::DpadUp, Key::Up},
        std::pair{GamepadButton::LeftStickUp, Key::Up},
        std::pair{GamepadButton::DpadDown, Key::Down},
        std::pair{GamepadButton::LeftStickDown, Key::Down},
        std::pair{GamepadButton::DpadLeft, Key::Left},
        std::pair{GamepadButton::LeftStickLeft, Key::Left},
        std::pair{GamepadButton::DpadRight, Key::Right},
        std::pair{GamepadButton::LeftStickRight, Key::Right},
        std::pair{GamepadButton::South, Key::Enter},
        std::pair{GamepadButton::East, Key::Escape},
    };
    const auto pressed = keyboard.pressed_keys();
    std::vector<Key> keys{pressed.begin(), pressed.end()};
    for (const auto& [button, key] : buttons) {
        if (gamepad.pressed(button) && !std::ranges::contains(keys, key)) {
            keys.push_back(key);
        }
    }
    return keys;
}

} // namespace rtype
