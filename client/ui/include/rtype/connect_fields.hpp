/// @file
/// @brief The connect screen's fields as the engine's text fields, typed through the
/// platform's text input.
#pragma once
#include <aegis2d/platform/keyboard.hpp>
#include <aegis2d/platform/text_input.hpp>
#include <aegis2d/render/bitmap_font.hpp>
#include <aegis2d/render/renderer.hpp>
#include <aegis2d/ui/text_field.hpp>
#include <aegis2d/ui/widget_tree.hpp>

#include <rtype/client_session.hpp>
#include <rtype/connect_form.hpp>

#include <array>
#include <optional>
#include <string>

namespace rtype {

/// @brief The server, port and name fields of the connect screen, as ui::TextField.
/// @details Each field is one of the engine's text fields: the text comes from the
/// platform's text input, so it follows the keyboard layout (an address typed on an AZERTY
/// keyboard has its dots and colons), Left, Right, Home and End move the cursor, and
/// Backspace and Delete erase at it. What a field takes, and its length, are the
/// ConnectForm's rules: the typed characters a field does not take are left out before they
/// reach it. The form keeps the texts, so its submit() checks them before joining.
class ConnectFields {
public:
    /// @brief Creates the fields, filled with settings, none focused.
    /// @details The settings go through the form's rules first, as if typed: characters a
    /// field does not take are left out and the rest cut to its length, so the fields show
    /// exactly what joining would send.
    /// @param settings Server and player name shown at first.
    explicit ConnectFields(const ClientSettings& settings);

    /// @brief Focuses a field, or none, as when a control row of the screen is selected.
    /// @details A field that gains the focus has its cursor put at the end of its text.
    /// @param field Field to edit, or nothing.
    void focus(std::optional<ConnectField> field);

    /// @brief The field being edited.
    /// @return It, or nothing.
    [[nodiscard]] std::optional<ConnectField> focused() const noexcept;

    /// @brief Applies a frame's typing to the focused field.
    /// @param typed Text typed during the frame, from Window::text_input().
    /// @param keyboard The frame's keyboard, for Left, Right, Home, End, Backspace and Delete.
    /// @return `true` if the field's text changed.
    bool update(const aegis2d::platform::TextInput& typed,
                const aegis2d::platform::Keyboard& keyboard);

    /// @brief Text of a field.
    /// @param field Field to read.
    /// @return Its text, as the form keeps it.
    [[nodiscard]] const std::string& text(ConnectField field) const;

    /// @brief The texts and their rules, to submit them.
    /// @return The form.
    [[nodiscard]] const ConnectForm& form() const noexcept;

    /// @brief Draws a field in its box: background, text, and the cursor while focused.
    /// @param renderer Renderer, between begin_frame() and end_frame().
    /// @param font Font of the text.
    /// @param field Field to draw.
    /// @param box Where it goes, in logical pixels.
    /// @param style Colors, text size and layer.
    void draw(aegis2d::render::IRenderer& renderer, const aegis2d::render::BitmapFont& font,
              ConnectField field, aegis2d::render::Rect box,
              const aegis2d::ui::TextFieldStyle& style);

private:
    [[nodiscard]] static ConnectForm checked(const ClientSettings& settings);
    [[nodiscard]] aegis2d::ui::TextField& field(ConnectField which);

    ConnectForm form_;
    aegis2d::ui::WidgetTree tree_;
    std::array<aegis2d::ui::TextField, connect_field_count> fields_;
};

} // namespace rtype
