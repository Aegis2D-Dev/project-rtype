#include <catch2/catch_test_macros.hpp>
#include <rtype/client_session.hpp>
#include <rtype/connect_form.hpp>
#include <rtype/room_list_form.hpp>
#include <rtype/rooms.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

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

TEST_CASE("the room list form keeps its selection and makes the request of a new room",
          "[client][rooms]") {
    using rtype::RoomRecord;
    rtype::RoomListForm form;
    CHECK(form.creating());
    CHECK(std::get<std::string>(form.submit()) == "Type a name for the new room.");
    form.update(std::vector<RoomRecord>{
        {.room = 0, .name = "Room 1", .players = 4, .max_players = 4, .state = 1},
        {.room = 2, .name = "Aces", .players = 1, .max_players = 2, .state = 0}});
    // The row of a new room stays the last one; up and down wrap around.
    CHECK(form.row() == 2);
    form.down();
    CHECK(form.row() == 0);
    CHECK(std::get<std::string>(form.submit()) == "This room is full.");
    form.down();
    CHECK(std::get<std::uint8_t>(form.submit()) == 2);
    CHECK(!form.type('x'));

    // The selected room stays selected when the list changes, as long as it is listed.
    form.update(std::vector<RoomRecord>{
        {.room = 1, .name = "Bees", .players = 0, .max_players = 4, .state = 0},
        {.room = 2, .name = "Aces", .players = 1, .max_players = 2, .state = 0}});
    CHECK(form.row() == 1);
    form.update(std::vector<RoomRecord>{});
    CHECK(form.creating());

    // A new room: a name of printable characters, up to 16, and 1 to 4 players.
    form.up();
    CHECK(form.creating());
    for (const char character : std::string{"Aces of the Bydo!"}) {
        static_cast<void>(form.type(character));
    }
    CHECK(form.name() == "Aces of the Bydo");
    CHECK(!form.type('\n'));
    form.erase();
    CHECK(form.size() == 4);
    form.bigger();
    CHECK(form.size() == 4);
    for (int press = 0; press < 5; ++press) {
        form.smaller();
    }
    CHECK(form.size() == 1);
    form.bigger();
    CHECK(std::get<rtype::CreateRoomRequest>(form.submit()) ==
          rtype::CreateRoomRequest{.name = "Aces of the Byd", .max_players = 2});
}
