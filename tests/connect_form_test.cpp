#include <catch2/catch_test_macros.hpp>
#include <rtype/client_session.hpp>
#include <rtype/connect_form.hpp>

#include <cstdint>
#include <string>
#include <variant>

using rtype::ClientSettings;
using rtype::ConnectField;
using rtype::ConnectForm;

namespace {

void type(ConnectForm& form, const std::string& text) {
    for (const char character : text) {
        form.type(character);
    }
}

void clear(ConnectForm& form) {
    while (!form.text(form.focused()).empty()) {
        form.erase();
    }
}

} // namespace

TEST_CASE("the connect form starts with the command line's settings", "[client][connect]") {
    const ConnectForm form{ClientSettings{.host = "10.0.0.2", .tcp_port = 5000, .name = "R9"}};
    CHECK(form.text(ConnectField::Host) == "10.0.0.2");
    CHECK(form.text(ConnectField::Port) == "5000");
    CHECK(form.text(ConnectField::Name) == "R9");
    CHECK(form.focused() == ConnectField::Name);
}

TEST_CASE("typed host, port and name reach the session's settings", "[client][connect]") {
    ConnectForm form{ClientSettings{}};
    form.focus(ConnectField::Host);
    clear(form);
    type(form, "192.168.1.20");
    form.next();
    CHECK(form.focused() == ConnectField::Port);
    clear(form);
    type(form, "4343");
    form.next();
    clear(form);
    type(form, "Lolo");

    const auto submitted = form.submit();
    REQUIRE(std::holds_alternative<ClientSettings>(submitted));
    const ClientSettings settings = std::get<ClientSettings>(submitted);
    CHECK(settings.host == "192.168.1.20");
    CHECK(settings.tcp_port == 4343);
    CHECK(settings.name == "Lolo");
}

TEST_CASE("each field only takes its own characters", "[client][connect]") {
    ConnectForm form{ClientSettings{.host = "", .tcp_port = 1, .name = ""}};
    form.focus(ConnectField::Port);
    form.erase();
    CHECK_FALSE(form.type('a'));
    CHECK(form.type('6'));
    type(form, "5535123");
    CHECK(form.text(ConnectField::Port) == "65535"); // five digits at most

    form.focus(ConnectField::Host);
    CHECK_FALSE(form.type(' '));
    type(form, "[::1]");
    CHECK(form.text(ConnectField::Host) == "[::1]");

    form.focus(ConnectField::Name);
    CHECK_FALSE(form.type('\n'));
    type(form, "Captain R-9 _01234567");
    CHECK(form.text(ConnectField::Name).size() == ConnectForm::max_name);

    form.previous();
    CHECK(form.focused() == ConnectField::Port);
    form.previous();
    form.previous();
    CHECK(form.focused() == ConnectField::Name);
}

TEST_CASE("a wrong field is told to the player", "[client][connect]") {
    ConnectForm form{ClientSettings{}};
    form.focus(ConnectField::Name);
    clear(form);
    CHECK(std::get<std::string>(form.submit()) == "Enter a name of 1 to 16 characters.");

    form.type('A');
    form.focus(ConnectField::Port);
    clear(form);
    type(form, "70000");
    CHECK(std::get<std::string>(form.submit()) == "The port is a number from 1 to 65535.");

    form.focus(ConnectField::Host);
    clear(form);
    CHECK(std::get<std::string>(form.submit()) == "Enter the address of the server.");
}

TEST_CASE("the rules of each field are told and kept by set_text", "[client][connect]") {
    CHECK(ConnectForm::accepts(ConnectField::Host, ':'));
    CHECK_FALSE(ConnectForm::accepts(ConnectField::Host, ' '));
    CHECK(ConnectForm::accepts(ConnectField::Port, '7'));
    CHECK_FALSE(ConnectForm::accepts(ConnectField::Port, '.'));
    CHECK(ConnectForm::accepts(ConnectField::Name, ' '));
    CHECK_FALSE(ConnectForm::accepts(ConnectField::Name, '\t'));
    // The bytes of a longer UTF-8 character, such as the first of 'é', are a name's only.
    CHECK(ConnectForm::accepts(ConnectField::Name, static_cast<char>(0xC3)));
    CHECK_FALSE(ConnectForm::accepts(ConnectField::Host, static_cast<char>(0xC3)));
    CHECK(ConnectForm::max_length(ConnectField::Host) == ConnectForm::max_host);
    CHECK(ConnectForm::max_length(ConnectField::Port) == ConnectForm::max_port);
    CHECK(ConnectForm::max_length(ConnectField::Name) == ConnectForm::max_name);

    ConnectForm form{ClientSettings{.host = "", .tcp_port = 1, .name = ""}};
    CHECK(form.set_text(ConnectField::Host, "10.0.0.7"));
    CHECK(form.text(ConnectField::Host) == "10.0.0.7");
    // What the field does not take, or holds no more of, is left out.
    CHECK_FALSE(form.set_text(ConnectField::Port, "80a80"));
    CHECK(form.text(ConnectField::Port) == "8080");
    CHECK_FALSE(form.set_text(ConnectField::Port, "1234567"));
    CHECK(form.text(ConnectField::Port) == "12345");
    CHECK(form.set_text(ConnectField::Name, ""));
    CHECK(form.text(ConnectField::Name).empty());

    // Names are UTF-8, as the login takes them, and cut to 16 bytes between characters.
    CHECK(form.set_text(ConnectField::Name, "Zoé"));
    CHECK(form.text(ConnectField::Name) == "Zoé");
    CHECK_FALSE(form.set_text(ConnectField::Name, "ééééééééé"));
    CHECK(form.text(ConnectField::Name) == "éééééééé");
    CHECK_FALSE(form.set_text(ConnectField::Name, "abcdefghijklmnoé"));
    CHECK(form.text(ConnectField::Name) == "abcdefghijklmno");
    // One byte of a longer character cannot be typed alone.
    form.focus(ConnectField::Name);
    CHECK_FALSE(form.type(static_cast<char>(0xC3)));
}
