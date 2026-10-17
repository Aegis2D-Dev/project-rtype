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
