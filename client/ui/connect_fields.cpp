#include <aegis2d/platform/keyboard.hpp>
#include <aegis2d/platform/text_input.hpp>
#include <aegis2d/ui/text_field.hpp>
#include <aegis2d/ui/widget_tree.hpp>

#include <rtype/client_session.hpp>
#include <rtype/connect_fields.hpp>
#include <rtype/connect_form.hpp>

#include <cstddef>
#include <optional>
#include <string>

namespace rtype {

namespace platform = aegis2d::platform;
namespace render = aegis2d::render;
namespace ui = aegis2d::ui;

namespace {

std::size_t index_of(ConnectField field) noexcept {
    return static_cast<std::size_t>(field);
}

// A text field of the tree, with the form's text and length; its box is set when drawn.
ui::TextField make_field(ui::WidgetTree& tree, const ConnectForm& form, ConnectField which) {
    return ui::TextField{tree,
                         {},
                         {.text = form.text(which),
                          .placeholder = {},
                          .max_length = ConnectForm::max_length(which)}};
}

} // namespace

ConnectFields::ConnectFields(const ClientSettings& settings)
    : form_{settings}, fields_{make_field(tree_, form_, ConnectField::Host),
                               make_field(tree_, form_, ConnectField::Port),
                               make_field(tree_, form_, ConnectField::Name)} {}

ui::TextField& ConnectFields::field(ConnectField which) {
    return fields_.at(index_of(which));
}

void ConnectFields::focus(std::optional<ConnectField> field) {
    if (field.has_value()) {
        tree_.focus(fields_.at(index_of(*field)).id());
    } else {
        tree_.clear_focus();
    }
}

std::optional<ConnectField> ConnectFields::focused() const noexcept {
    const std::optional<ui::WidgetId> widget = tree_.focused();
    if (!widget.has_value()) {
        return std::nullopt;
    }
    std::size_t index = 0;
    for (const ui::TextField& field : fields_) {
        if (field.id() == *widget) {
            return static_cast<ConnectField>(index);
        }
        ++index;
    }
    return std::nullopt;
}

bool ConnectFields::update(const platform::TextInput& typed, const platform::Keyboard& keyboard) {
    const std::optional<ConnectField> which = focused();
    if (!which.has_value()) {
        return false;
    }
    // Only what the field takes reaches it: a letter typed in the port is left out.
    std::string kept;
    for (const char character : typed.text()) {
        if (ConnectForm::accepts(*which, character)) {
            kept.push_back(character);
        }
    }
    platform::TextInput filtered;
    filtered.set_active(true);
    filtered.add(kept);
    // Left and Right move the cursor; Up, Down and Tab stay the screen's, to change rows.
    ui::Navigation navigation;
    if (keyboard.pressed(platform::Key::Left)) {
        navigation.direction = ui::Direction::Left;
    } else if (keyboard.pressed(platform::Key::Right)) {
        navigation.direction = ui::Direction::Right;
    }
    ui::TextField& edited = field(*which);
    if (!edited.update(navigation, filtered, keyboard, tree_)) {
        return false;
    }
    form_.set_text(*which, edited.text());
    return true;
}

const std::string& ConnectFields::text(ConnectField field) const {
    return form_.text(field);
}

const ConnectForm& ConnectFields::form() const noexcept {
    return form_;
}

void ConnectFields::draw(render::IRenderer& renderer, const render::BitmapFont& font,
                         ConnectField which, render::Rect box, const ui::TextFieldStyle& style) {
    ui::TextField& drawn = field(which);
    tree_.set_bounds(drawn.id(), box);
    drawn.draw(renderer, tree_, font, style);
}

} // namespace rtype
