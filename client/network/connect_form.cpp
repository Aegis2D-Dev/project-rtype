#include <rtype/client_options.hpp>
#include <rtype/client_session.hpp>
#include <rtype/connect_form.hpp>
#include <rtype/login.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

// Printable ASCII, or a byte of a longer UTF-8 character: names are UTF-8, as the login
// takes them (valid_player_name() checks the whole name before joining).
bool name_character(char character) noexcept {
    return (character >= ' ' && character <= '~') || static_cast<unsigned char>(character) >= 0x80U;
}

// Bytes of the UTF-8 character that starts with @p lead; 1 for a byte that starts none.
std::size_t sequence_size(char lead) noexcept {
    const auto byte = static_cast<unsigned char>(lead);
    if (byte >= 0xF0U && byte <= 0xF7U) {
        return 4;
    }
    if (byte >= 0xE0U) {
        return byte <= 0xEFU ? 3 : 1;
    }
    return byte >= 0xC0U ? 2 : 1;
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

bool ConnectForm::accepts(ConnectField field, char character) noexcept {
    switch (field) {
    case ConnectField::Host:
        return host_character(character);
    case ConnectField::Port:
        return digit(character);
    case ConnectField::Name:
        return name_character(character);
    }
    return false;
}

std::size_t ConnectForm::max_length(ConnectField field) noexcept {
    switch (field) {
    case ConnectField::Host:
        return max_host;
    case ConnectField::Port:
        return max_port;
    case ConnectField::Name:
        return max_name;
    }
    return 0;
}

bool ConnectForm::type(char character) {
    std::string& field = fields_.at(index_of(focused_));
    // One byte is one character only in ASCII: longer ones go through set_text().
    if (static_cast<unsigned char>(character) >= 0x80U || !accepts(focused_, character) ||
        field.size() >= max_length(focused_)) {
        return false;
    }
    field.push_back(character);
    return true;
}

bool ConnectForm::set_text(ConnectField field, std::string_view text) {
    std::string kept;
    bool full = false;
    // Character by character, so a field is never cut inside a UTF-8 character.
    for (std::size_t start = 0; start < text.size();) {
        const std::size_t size = std::min(sequence_size(text.at(start)), text.size() - start);
        const std::string_view character = text.substr(start, size);
        start += size;
        if (!std::ranges::all_of(character, [field](char byte) { return accepts(field, byte); })) {
            continue;
        }
        full = full || kept.size() + character.size() > max_length(field);
        if (!full) {
            kept.append(character);
        }
    }
    const bool whole = kept.size() == text.size();
    fields_.at(index_of(field)) = std::move(kept);
    return whole;
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
