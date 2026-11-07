#include <aegis2d/platform/gamepad.hpp>
#include <aegis2d/platform/keyboard.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/menu_keys.hpp>

#include <vector>

using aegis2d::platform::Gamepad;
using aegis2d::platform::GamepadButton;
using aegis2d::platform::Key;
using aegis2d::platform::Keyboard;

TEST_CASE("the lobby takes a gamepad's D-pad, stick, A and B as its keys", "[client][a11y]") {
    Keyboard keyboard;
    Gamepad gamepad;
    gamepad.connect(aegis2d::platform::GamepadStyle::Xbox);
    keyboard.begin_frame();
    gamepad.begin_frame();
    CHECK(rtype::menu_keys(keyboard, gamepad).empty());

    // Choose a room with the D-pad, then join it with A.
    gamepad.press(GamepadButton::DpadDown);
    CHECK(rtype::menu_keys(keyboard, gamepad) == std::vector{Key::Down});
    gamepad.begin_frame();
    CHECK(rtype::menu_keys(keyboard, gamepad).empty());
    gamepad.press(GamepadButton::South);
    CHECK(rtype::menu_keys(keyboard, gamepad) == std::vector{Key::Enter});

    // The stick moves as the D-pad does, and B goes back.
    gamepad.begin_frame();
    gamepad.press(GamepadButton::LeftStickUp);
    gamepad.press(GamepadButton::LeftStickRight);
    gamepad.press(GamepadButton::East);
    CHECK(rtype::menu_keys(keyboard, gamepad) == std::vector{Key::Up, Key::Right, Key::Escape});

    // The fire and option buttons are not menu keys.
    gamepad.begin_frame();
    gamepad.press(GamepadButton::RightTrigger);
    gamepad.press(GamepadButton::Back);
    CHECK(rtype::menu_keys(keyboard, gamepad).empty());
}

TEST_CASE("the keyboard's keys come first, and a key and its button count once", "[client][a11y]") {
    Keyboard keyboard;
    Gamepad gamepad;
    gamepad.connect(aegis2d::platform::GamepadStyle::Xbox);
    keyboard.begin_frame();
    gamepad.begin_frame();
    keyboard.press(Key::A);
    keyboard.press(Key::Enter);
    gamepad.press(GamepadButton::South);
    gamepad.press(GamepadButton::DpadLeft);
    gamepad.press(GamepadButton::LeftStickLeft);
    CHECK(rtype::menu_keys(keyboard, gamepad) == std::vector{Key::A, Key::Enter, Key::Left});
}
