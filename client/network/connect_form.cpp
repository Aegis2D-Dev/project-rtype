#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/connect_form.hpp>
#include <rtype/login.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace rtype {

namespace {

bool digit(char character) noexcept {
    return character >= '0' && character <= '9';
}

bool letter(char character) noexcept {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

bool host_character(char character) noexcept {
    return letter(character) || digit(character) || character == '.' || character == ':' ||
           character == '-' || character == '[' || character == ']';
}

bool name_character(char character) noexcept {
    return character >= ' ' && character <= '~';
}

std::size_t index_of(ConnectField field) noexcept {
    return static_cast<std::size_t>(field);
}

} // namespace

ConnectForm::ConnectForm(const ClientSettings& settings)
    : fields_{settings.host, std::to_string(settings.tcp_port), settings.name} {}

ConnectField ConnectForm::focused() const noexcept {
    return focused_;
}

void ConnectForm::focus(ConnectField field) noexcept {
    focused_ = field;
}

void ConnectForm::next() noexcept {
    focused_ = static_cast<ConnectField>((index_of(focused_) + 1) % connect_field_count);
}

void ConnectForm::previous() noexcept {
    focused_ = static_cast<ConnectField>((index_of(focused_) + connect_field_count - 1) %
                                         connect_field_count);
}

bool ConnectForm::type(char character) {
    std::string& field = fields_.at(index_of(focused_));
    bool allowed = false;
    std::size_t longest = 0;
    switch (focused_) {
    case ConnectField::Host:
        allowed = host_character(character);
        longest = max_host;
        break;
    case ConnectField::Port:
        allowed = digit(character);
        longest = max_port;
        break;
    case ConnectField::Name:
        allowed = name_character(character);
        longest = max_name;
        break;
    }
    if (!allowed || field.size() >= longest) {
        return false;
    }
    field.push_back(character);
    return true;
}

void ConnectForm::erase() noexcept {
    std::string& field = fields_.at(index_of(focused_));
    if (!field.empty()) {
        field.pop_back();
    }
}

const std::string& ConnectForm::text(ConnectField field) const {
    return fields_.at(index_of(field));
}

std::variant<ClientSettings, std::string> ConnectForm::submit() const {
    const std::string& host = text(ConnectField::Host);
    if (host.empty()) {
        return std::string{"Enter the address of the server."};
    }
    const std::optional<std::uint16_t> port = parse_port(text(ConnectField::Port));
    if (!port.has_value()) {
        return std::string{"The port is a number from 1 to 65535."};
    }
    const std::string& name = text(ConnectField::Name);
    if (!valid_player_name(name)) {
        return std::string{"Enter a name of 1 to 16 characters."};
    }
    return ClientSettings{.host = host, .tcp_port = *port, .name = name};
}

} // namespace rtype
