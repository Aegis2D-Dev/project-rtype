#include <aegis2d/platform/keyboard.hpp>
#include <aegis2d/platform/text_input.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/color.hpp>
#include <aegis2d/render/headless_renderer.hpp>
#include <aegis2d/render/renderer.hpp>
#include <aegis2d/render/texture_manager.hpp>
#include <aegis2d/ui/text_field.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/client_session.hpp>
#include <rtype/connect_fields.hpp>
#include <rtype/connect_form.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using aegis2d::platform::Key;
using aegis2d::platform::Keyboard;
using aegis2d::platform::TextInput;
using aegis2d::render::BitmapFont;
using aegis2d::render::HeadlessRenderer;
using aegis2d::render::no_texture;
using aegis2d::render::Rect;
using aegis2d::render::Sprite;
using aegis2d::render::TextureManager;
using rtype::ClientSettings;
using rtype::ConnectField;
using rtype::ConnectFields;
using rtype::ConnectForm;

namespace {

// The command line's settings by default.
ClientSettings defaults() {
    return {.host = "127.0.0.1", .tcp_port = 4242, .name = "player"};
}

// One frame of the connect screen: the text the platform says was typed, the keys pressed.
bool frame(ConnectFields& fields, std::string_view typed, const std::vector<Key>& keys = {}) {
    TextInput input;
    input.set_active(true);
    input.add(typed);
    Keyboard keyboard;
    keyboard.begin_frame();
    for (const Key key : keys) {
        keyboard.press(key);
        keyboard.release(key);
    }
    return fields.update(input, keyboard);
}

// Erases a field from its end, as many Backspaces as it has characters.
void clear(ConnectFields& fields, ConnectField field) {
    const std::size_t length = fields.text(field).size();
    for (std::size_t index = 0; index < length; ++index) {
        frame(fields, "", {Key::Backspace});
    }
}

} // namespace

TEST_CASE("the fields start with the settings and none edited", "[client][connect]") {
    ConnectFields fields{defaults()};
    CHECK(fields.text(ConnectField::Host) == "127.0.0.1");
    CHECK(fields.text(ConnectField::Port) == "4242");
    CHECK(fields.text(ConnectField::Name) == "player");
    CHECK_FALSE(fields.focused().has_value());

    // Typing with no field selected, as on a control row, changes nothing.
    CHECK_FALSE(frame(fields, "lost"));
    fields.focus(ConnectField::Port);
    CHECK(fields.focused() == ConnectField::Port);
    fields.focus(std::nullopt);
    CHECK_FALSE(frame(fields, "8080"));
    CHECK(fields.text(ConnectField::Port) == "4242");
}

TEST_CASE("an address typed on any layout reaches the server field", "[client][connect]") {
    ConnectFields fields{defaults()};
    fields.focus(ConnectField::Host);
    clear(fields, ConnectField::Host);
    CHECK(fields.text(ConnectField::Host).empty());

    // What the platform's text input gives for an address typed on an AZERTY keyboard: the
    // characters of the layout, dots and colons included, which raw keys would get wrong.
    CHECK(frame(fields, "192.168.1.10"));
    CHECK(fields.text(ConnectField::Host) == "192.168.1.10");
    clear(fields, ConnectField::Host);
    CHECK(frame(fields, "[::1]"));
    CHECK(fields.text(ConnectField::Host) == "[::1]");
    // What the server field does not take is left out: a space, a slash, an accent.
    CHECK_FALSE(frame(fields, " /é"));
    CHECK(fields.text(ConnectField::Host) == "[::1]");
}

TEST_CASE("each field keeps to its own characters and length", "[client][connect]") {
    ConnectFields fields{defaults()};
    fields.focus(ConnectField::Port);
    clear(fields, ConnectField::Port);
    CHECK(frame(fields, "8a0b8c0"));
    CHECK(fields.text(ConnectField::Port) == "8080");
    frame(fields, "123456");
    CHECK(fields.text(ConnectField::Port).size() == ConnectForm::max_port);

    fields.focus(ConnectField::Name);
    clear(fields, ConnectField::Name);
    CHECK(frame(fields, "Ace Pilot 2"));
    CHECK(fields.text(ConnectField::Name) == "Ace Pilot 2");
    // Accented letters too, as the login takes UTF-8, up to 16 bytes: never half a letter.
    frame(fields, "éèà, and more");
    CHECK(fields.text(ConnectField::Name) == "Ace Pilot 2éè");
    // No control character.
    CHECK_FALSE(frame(fields, "\t"));
}

TEST_CASE("a field is edited at its cursor", "[client][connect]") {
    ConnectFields fields{defaults()};
    fields.focus(ConnectField::Host);
    // The cursor starts at the end: two to the left, before ".1", then a digit typed there.
    frame(fields, "", {Key::Left});
    frame(fields, "", {Key::Left});
    CHECK(frame(fields, "2"));
    CHECK(fields.text(ConnectField::Host) == "127.0.02.1");
    // Backspace erases before the cursor, Delete after it; Home and End jump.
    frame(fields, "", {Key::Backspace});
    CHECK(fields.text(ConnectField::Host) == "127.0.0.1");
    frame(fields, "", {Key::Home});
    frame(fields, "", {Key::Delete});
    CHECK(fields.text(ConnectField::Host) == "27.0.0.1");
    frame(fields, "", {Key::End});
    frame(fields, "0");
    CHECK(fields.text(ConnectField::Host) == "27.0.0.10");
}

TEST_CASE("a full name refuses what is typed, wherever the cursor is", "[client][connect]") {
    // Eight accented letters: 8 characters, but the 16 bytes a name holds.
    ConnectFields fields{ClientSettings{.host = "::1", .tcp_port = 4242, .name = "éééééééé"}};
    fields.focus(ConnectField::Name);
    frame(fields, "", {Key::Home});
    CHECK_FALSE(frame(fields, "a"));
    CHECK(fields.text(ConnectField::Name) == "éééééééé");
    // The cursor stayed at the start: Delete takes the first letter, and there is room again.
    frame(fields, "", {Key::Delete});
    // Two bytes free: "ab" fits, typed at the start; a third letter would not.
    CHECK(frame(fields, "abc"));
    CHECK(fields.text(ConnectField::Name) == "abééééééé");
}

TEST_CASE("an accented name typed is joined with", "[client][connect]") {
    ConnectFields fields{defaults()};
    fields.focus(ConnectField::Name);
    clear(fields, ConnectField::Name);
    CHECK(frame(fields, "Zoé"));
    const auto settings = fields.form().submit();
    REQUIRE(std::holds_alternative<ClientSettings>(settings));
    CHECK(std::get<ClientSettings>(settings).name == "Zoé");
}

TEST_CASE("the typed fields are what the client joins with", "[client][connect]") {
    ConnectFields fields{defaults()};
    fields.focus(ConnectField::Port);
    clear(fields, ConnectField::Port);
    frame(fields, "5000");
    fields.focus(ConnectField::Name);
    frame(fields, "2");
    const auto settings = fields.form().submit();
    REQUIRE(std::holds_alternative<ClientSettings>(settings));
    CHECK(std::get<ClientSettings>(settings).host == "127.0.0.1");
    CHECK(std::get<ClientSettings>(settings).tcp_port == 5000);
    CHECK(std::get<ClientSettings>(settings).name == "player2");

    // A field the server cannot take is told before joining.
    fields.focus(ConnectField::Port);
    clear(fields, ConnectField::Port);
    CHECK(std::holds_alternative<std::string>(fields.form().submit()));
}

TEST_CASE("a long address is drawn inside its box", "[client][connect]") {
    ConnectFields fields{defaults()};
    fields.focus(ConnectField::Host);
    clear(fields, ConnectField::Host);
    frame(fields, std::string(ConnectForm::max_host, '9'));
    REQUIRE(fields.text(ConnectField::Host).size() == ConnectForm::max_host);

    HeadlessRenderer renderer;
    TextureManager textures{renderer};
    const auto font = BitmapFont::builtin(textures);
    const Rect box{.x = 300.0F, .y = 200.0F, .width = 400.0F, .height = 40.0F};
    const aegis2d::ui::TextFieldStyle style{.text_scale = 2.5F};
    renderer.begin_frame(aegis2d::render::rgb(0x000000));
    fields.draw(renderer, font, ConnectField::Host, box, style);
    fields.draw(renderer, font, ConnectField::Port,
                {.x = 300.0F, .y = 250.0F, .width = 400.0F, .height = 40.0F}, style);
    renderer.end_frame();

    std::size_t glyphs = 0;
    std::size_t cursors = 0;
    for (const Sprite& sprite : renderer.sprites()) {
        if (sprite.texture != no_texture && sprite.position.y < 250.0F) {
            // The host's glyphs: scrolled to keep the cursor in view, none past the box.
            CHECK(sprite.position.x >= box.x);
            CHECK(sprite.position.x + sprite.size.x <= box.x + box.width);
            ++glyphs;
        }
        if (sprite.texture == no_texture && sprite.size.x == style.outline_width) {
            ++cursors;
        }
    }
    CHECK(glyphs > 0);
    CHECK(glyphs < ConnectForm::max_host);
    // Only the field being edited has a cursor.
    CHECK(cursors == 1);
}

TEST_CASE("a field gained again is edited from its end", "[client][connect]") {
    ConnectFields fields{defaults()};
    fields.focus(ConnectField::Host);
    frame(fields, "", {Key::Home});
    // Away to the port and back: typing goes at the end, not where the cursor was left.
    fields.focus(ConnectField::Port);
    fields.focus(ConnectField::Host);
    frame(fields, "9");
    CHECK(fields.text(ConnectField::Host) == "127.0.0.19");
    // Focusing the field it already edits keeps its cursor.
    frame(fields, "", {Key::Left});
    fields.focus(ConnectField::Host);
    frame(fields, "8");
    CHECK(fields.text(ConnectField::Host) == "127.0.0.189");
}

TEST_CASE("command line settings are shown as joining would send them", "[client][connect]") {
    // What --host and --name may hold that the fields do not take.
    ConnectFields fields{
        ClientSettings{.host = "my host", .tcp_port = 4242, .name = "Zoé the ace pilot X9"}};
    CHECK(fields.text(ConnectField::Host) == "myhost");
    // 16 bytes: the 'é' takes two.
    CHECK(fields.text(ConnectField::Name) == "Zoé the ace pil");
    CHECK(fields.text(ConnectField::Name).size() == ConnectForm::max_name);

    // Edited, the field and the form stay the same text, which is what joining sends.
    fields.focus(ConnectField::Name);
    frame(fields, "", {Key::Backspace});
    frame(fields, "s");
    CHECK(fields.text(ConnectField::Name) == "Zoé the ace pis");
    // Typed past 16 bytes, the field keeps to what the form keeps.
    frame(fields, "é");
    CHECK(fields.text(ConnectField::Name) == "Zoé the ace pis");
    const auto settings = fields.form().submit();
    REQUIRE(std::holds_alternative<ClientSettings>(settings));
    CHECK(std::get<ClientSettings>(settings).name == "Zoé the ace pis");
    CHECK(std::get<ClientSettings>(settings).host == "myhost");

    // What is drawn is that text too: as many glyphs as it has characters.
    HeadlessRenderer renderer;
    TextureManager textures{renderer};
    const auto font = BitmapFont::builtin(textures);
    renderer.begin_frame(aegis2d::render::rgb(0x000000));
    fields.draw(renderer, font, ConnectField::Host,
                {.x = 0.0F, .y = 0.0F, .width = 600.0F, .height = 40.0F}, {});
    renderer.end_frame();
    std::size_t glyphs = 0;
    for (const Sprite& sprite : renderer.sprites()) {
        glyphs += sprite.texture != no_texture ? 1 : 0;
    }
    CHECK(glyphs == std::string{"myhost"}.size());
}
