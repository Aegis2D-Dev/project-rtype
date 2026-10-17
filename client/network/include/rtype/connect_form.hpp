/// @file
/// @brief The fields of the connect screen: server address, port and player name.
#pragma once
#include <rtype/client_session.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace rtype {

/// @brief A field of the connect screen.
enum class ConnectField : std::uint8_t {
    /// @brief Address of the server.
    Host,
    /// @brief TCP port of the server.
    Port,
    /// @brief Player name.
    Name,
};

/// @brief Number of fields of ConnectField.
inline constexpr std::size_t connect_field_count = 3;

/// @brief What the player types on the connect screen, checked before joining.
/// @details The form only keeps text: the client turns the keys pressed into characters
/// and gives them to type(), which refuses those the focused field cannot hold (a letter
/// in the port, a field already full). submit() checks the fields and gives the settings
/// of ClientSession, or a sentence for the player. Nothing here draws or reads the keyboard,
/// so the form is the same with any window.
class ConnectForm {
public:
    /// @brief Longest address: an IPv6 address with its brackets fits.
    static constexpr std::size_t max_host = 45;
    /// @brief Longest port: five digits.
    static constexpr std::size_t max_port = 5;
    /// @brief Longest name, in bytes, as `LoginRequest` allows.
    static constexpr std::size_t max_name = 16;

    /// @brief Creates the form, filled with settings (the command line's, or the last ones
    /// used), the name focused.
    /// @param settings Server and player name shown at first.
    explicit ConnectForm(const ClientSettings& settings);

    /// @brief The focused field, which type() and erase() change.
    /// @return The field.
    [[nodiscard]] ConnectField focused() const noexcept;

    /// @brief Focuses a field.
    /// @param field Field to focus.
    void focus(ConnectField field) noexcept;

    /// @brief Focuses the next field, the first after the last one.
    void next() noexcept;

    /// @brief Focuses the previous field, the last before the first one.
    void previous() noexcept;

    /// @brief Adds a character at the end of the focused field.
    /// @details The address takes letters, digits, `.`, `:`, `-`, `[` and `]`; the port
    /// takes digits; the name takes printable ASCII characters (space included).
    /// @param character Character typed.
    /// @return `false` if the field does not take it or is full; the field is unchanged.
    bool type(char character);

    /// @brief Removes the last character of the focused field, if any.
    void erase() noexcept;

    /// @brief Text of a field.
    /// @param field Field to read.
    /// @return Its text.
    [[nodiscard]] const std::string& text(ConnectField field) const;

    /// @brief Checks the fields and gives the settings to join with.
    /// @details The address must not be empty, the port must be a number from 1 to 65535,
    /// and the name 1 to max_name characters. Whether the address is one the client can
    /// reach is only known when it joins: ClientSession::start() says so.
    /// @return The settings, or a sentence telling the player what to fix.
    [[nodiscard]] std::variant<ClientSettings, std::string> submit() const;

private:
    std::array<std::string, connect_field_count> fields_;
    ConnectField focused_ = ConnectField::Name;
};

} // namespace rtype
