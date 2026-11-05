#include <aegis2d/net/endpoint.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/login.hpp>
#include <rtype/rooms.hpp>
#include <rtype/sessions.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using aegis2d::net::Endpoint;
using rtype::BindResult;
using rtype::LoginAccepted;
using rtype::LoginRejection;
using rtype::LoginRequest;
using rtype::LogoutReason;
using rtype::RoomRecord;
using rtype::RoomRejection;
using rtype::RoomState;
using rtype::Session;
using rtype::SessionTable;
using rtype::TcpKind;

namespace {

std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for (const unsigned value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

Session accepted(SessionTable& table, std::string name) {
    const std::variant<Session, LoginRejection> result =
        table.login(LoginRequest{.version = rtype::protocol_version, .name = std::move(name)});
    REQUIRE(std::holds_alternative<Session>(result));
    return std::get<Session>(result);
}

LoginRejection refused(SessionTable& table, std::uint8_t version, std::string name) {
    const std::variant<Session, LoginRejection> result =
        table.login(LoginRequest{.version = version, .name = std::move(name)});
    REQUIRE(std::holds_alternative<LoginRejection>(result));
    return std::get<LoginRejection>(result);
}

} // namespace

TEST_CASE("TCP messages follow the RFC example", "[server][login]") {
    // RFC section 3.5, without the length prefixes.
    CHECK(rtype::write_login_request(LoginRequest{.version = 1, .name = "R9"}) ==
          bytes({0x01, 0x01, 0x02, 0x00, 0x52, 0x39}));
    CHECK(rtype::write_login_accepted(
              LoginAccepted{.session = 0xDEADBEEF, .player = 2, .udp_port = 4242}) ==
          bytes({0x02, 0xEF, 0xBE, 0xAD, 0xDE, 0x02, 0x92, 0x10}));
    CHECK(rtype::write_login_rejected(LoginRejection::NameTaken) == bytes({0x03, 0x04}));
    CHECK(rtype::write_logout(LogoutReason::Shutdown) == bytes({0x04, 0x01}));
}

TEST_CASE("a LoginRequest is read whole or not at all", "[server][login]") {
    const std::optional<LoginRequest> request =
        rtype::read_login_request(bytes({0x01, 0x01, 0x02, 0x00, 0x52, 0x39}));
    CHECK(request == std::optional{LoginRequest{.version = 1, .name = "R9"}});

    CHECK(!rtype::read_login_request({}).has_value());
    CHECK(!rtype::read_login_request(bytes({0x04, 0x01, 0x02, 0x00, 0x52, 0x39})).has_value());
    CHECK(!rtype::read_login_request(bytes({0x01, 0x01, 0x02, 0x00, 0x52})).has_value());
    CHECK(
        !rtype::read_login_request(bytes({0x01, 0x01, 0x02, 0x00, 0x52, 0x39, 0x00})).has_value());
    CHECK(!rtype::read_login_request(bytes({0x01, 0x01, 0xFF, 0xFF, 0x52, 0x39})).has_value());
    CHECK(!rtype::read_login_request(bytes({0x01})).has_value());
}

TEST_CASE("kinds and logouts are checked", "[server][login]") {
    CHECK(rtype::kind_of(bytes({0x01})) == TcpKind::LoginRequest);
    CHECK(rtype::kind_of(bytes({0x04, 0x00})) == TcpKind::Logout);
    CHECK(!rtype::kind_of({}).has_value());
    CHECK(!rtype::kind_of(bytes({0x00})).has_value());
    CHECK(rtype::kind_of(bytes({0x05})) == TcpKind::ListRooms);
    CHECK(rtype::kind_of(bytes({0x09, 0x01})) == TcpKind::RoomRejected);
    CHECK(rtype::kind_of(bytes({0x10})) == TcpKind::GameStart);
    CHECK(!rtype::kind_of(bytes({0x11})).has_value());

    CHECK(rtype::read_logout(bytes({0x04, 0x00})) == std::uint8_t{0});
    CHECK(rtype::read_logout(bytes({0x04, 0x09})) == std::uint8_t{9});
    CHECK(!rtype::read_logout(bytes({0x04})).has_value());
    CHECK(!rtype::read_logout(bytes({0x04, 0x00, 0x00})).has_value());
    CHECK(!rtype::read_logout(bytes({0x03, 0x00})).has_value());
}

TEST_CASE("player names are 1 to 16 bytes of UTF-8 without control characters", "[server][login]") {
    // U+0085 is a C1 control, which the RFC allows: only C0 controls and DEL are refused.
    for (const std::string_view name :
         {"R9", "a", "sixteen_bytes_ok", "Zoë", "日本語", "🚀ship", "a b~", "\xC2\x85"}) {
        CAPTURE(name);
        CHECK(rtype::valid_player_name(name));
    }
    for (const std::string_view name : {
             std::string_view{""},
             std::string_view{"seventeen_bytes_x"},
             std::string_view{"tab\there"},
             std::string_view{"nul\0", 4},
             std::string_view{"del\x7F"},
             std::string_view{"\x80"},             // continuation byte first
             std::string_view{"\xC3"},             // sequence cut short
             std::string_view{"\xC3\x28"},         // bad continuation
             std::string_view{"\xC0\xAF"},         // overlong '/'
             std::string_view{"\xE0\x80\xAF"},     // overlong '/'
             std::string_view{"\xED\xA0\x80"},     // surrogate
             std::string_view{"\xF4\x90\x80\x80"}, // above U+10FFFF
             std::string_view{"\xFF"},
         }) {
        CAPTURE(name);
        CHECK(!rtype::valid_player_name(name));
    }
}

TEST_CASE("logins get the lowest free player and a unique token", "[server][sessions]") {
    SessionTable table{42};
    std::vector<std::uint32_t> tokens;
    for (std::size_t player = 0; player < rtype::tuning::max_players; ++player) {
        const Session session = accepted(table, "p" + std::to_string(player));
        CHECK(session.player == player);
        CHECK(session.token != 0);
        tokens.push_back(session.token);
        const Session* const found = table.find(session.token);
        REQUIRE(found != nullptr);
        CHECK(found->name == session.name);
    }
    CHECK(std::set<std::uint32_t>(tokens.begin(), tokens.end()).size() == tokens.size());
    CHECK(table.size() == rtype::tuning::max_players);
    CHECK(table.find(0) == nullptr);
    CHECK(refused(table, rtype::protocol_version, "fifth") == LoginRejection::ServerFull);

    // Player 1 leaves: the next login takes its number, and its name is free again.
    CHECK(table.end(tokens.at(1)));
    CHECK(!table.end(tokens.at(1)));
    CHECK(table.find(tokens.at(1)) == nullptr);
    CHECK(table.size() == rtype::tuning::max_players - 1);
    const Session again = accepted(table, "p1");
    CHECK(again.player == 1);
    CHECK(again.token != 0);
}

TEST_CASE("logins are refused in the RFC order", "[server][sessions]") {
    SessionTable table{7};
    CHECK(refused(table, 2, "R9") == LoginRejection::BadVersion);
    CHECK(refused(table, 2, "") == LoginRejection::BadVersion);
    CHECK(refused(table, rtype::protocol_version, "") == LoginRejection::BadName);
    CHECK(refused(table, rtype::protocol_version, "a\nb") == LoginRejection::BadName);
    static_cast<void>(accepted(table, "R9"));
    CHECK(refused(table, rtype::protocol_version, "R9") == LoginRejection::NameTaken);
    CHECK(table.size() == 1);
}

TEST_CASE("logins fill the first room, then the next one", "[server][sessions]") {
    SessionTable table{11};
    CHECK(table.rooms() == 1);
    table.set_rooms(2);
    CHECK(table.rooms() == 2);
    std::vector<Session> sessions;
    for (std::size_t index = 0; index < 2 * rtype::tuning::max_players; ++index) {
        sessions.push_back(accepted(table, "p" + std::to_string(index)));
        CHECK(sessions.back().room == index / rtype::tuning::max_players);
        CHECK(sessions.back().player == index % rtype::tuning::max_players);
    }
    CHECK(refused(table, rtype::protocol_version, "ninth") == LoginRejection::ServerFull);
    // A name is taken in every room.
    CHECK(table.end(sessions.back().token));
    CHECK(refused(table, rtype::protocol_version, "p0") == LoginRejection::NameTaken);

    // A number freed in the first room goes before the one freed in the second.
    CHECK(table.end(sessions.at(2).token));
    const Session first = accepted(table, "first");
    CHECK(first.room == 0);
    CHECK(first.player == 2);
    const Session second = accepted(table, "second");
    CHECK(second.room == 1);
    CHECK(second.player == rtype::tuning::max_players - 1);

    // At least one room.
    SessionTable single{12};
    single.set_rooms(0);
    CHECK(single.rooms() == 1);
}

TEST_CASE("room messages follow the RFC example", "[server][rooms]") {
    // RFC section 3.6, without the length prefixes.
    CHECK(rtype::write_list_rooms() == bytes({0x05}));
    const std::vector<RoomRecord> rooms{
        {.room = 0, .name = "Room 1", .players = 1, .max_players = 4, .state = 1},
        {.room = 1, .name = "Aces", .players = 0, .max_players = 2, .state = 0}};
    const std::vector<std::byte> list =
        bytes({0x06, 0x02, 0x00, 0x06, 0x00, 0x52, 0x6F, 0x6F, 0x6D, 0x20, 0x31, 0x01,
               0x04, 0x01, 0x01, 0x04, 0x00, 0x41, 0x63, 0x65, 0x73, 0x00, 0x02, 0x00});
    CHECK(rtype::write_room_list(rooms) == list);
    CHECK(rtype::read_room_list(list) == rooms);
    CHECK(rtype::write_create_room({.name = "Aces", .max_players = 2}) ==
          bytes({0x07, 0x04, 0x00, 0x41, 0x63, 0x65, 0x73, 0x02}));
    CHECK(rtype::write_room_created(1) == bytes({0x08, 0x01}));
    CHECK(rtype::write_room_rejected(RoomRejection::BadSize) == bytes({0x09, 0x03}));
}

TEST_CASE("room messages are read whole or not at all", "[server][rooms]") {
    CHECK(rtype::read_list_rooms(bytes({0x05})));
    CHECK(!rtype::read_list_rooms(bytes({0x05, 0x00})));
    CHECK(!rtype::read_list_rooms(bytes({0x04})));

    CHECK(rtype::read_create_room(bytes({0x07, 0x01, 0x00, 0x41, 0x03})) ==
          rtype::CreateRoomRequest{.name = "A", .max_players = 3});
    CHECK(!rtype::read_create_room(bytes({0x07, 0x01, 0x00, 0x41})).has_value());
    CHECK(!rtype::read_create_room(bytes({0x07, 0x05, 0x00, 0x41, 0x03})).has_value());
    CHECK(!rtype::read_create_room(bytes({0x07, 0x01, 0x00, 0x41, 0x03, 0x00})).has_value());

    CHECK(rtype::read_room_list(bytes({0x06, 0x00})) == std::vector<RoomRecord>{});
    CHECK(!rtype::read_room_list(bytes({0x06})).has_value());
    CHECK(!rtype::read_room_list(bytes({0x06, 0x01, 0x00})).has_value());
    CHECK(!rtype::read_room_list(bytes({0x06, 0x00, 0x00})).has_value());
    CHECK(!rtype::read_room_list(bytes({0x06, 65})).has_value());
    CHECK_THROWS_AS(rtype::write_room_list(std::vector<RoomRecord>(65)), std::invalid_argument);

    CHECK(rtype::read_room_created(bytes({0x08, 0x03})) == std::uint8_t{3});
    CHECK(!rtype::read_room_created(bytes({0x08})).has_value());
    CHECK(rtype::read_room_rejected(bytes({0x09, 0x07})) == std::uint8_t{7});
    CHECK(!rtype::read_room_rejected(bytes({0x08, 0x01})).has_value());
}

TEST_CASE("rooms are created, listed, filled by the logins and end with their last player",
          "[server][sessions][rooms]") {
    SessionTable table{13};
    table.set_rooms(3);
    CHECK(table.list_rooms().empty());
    const auto create = [&](std::string name, std::uint8_t size) {
        return table.create_room({.name = std::move(name), .max_players = size});
    };
    const auto room_of = [](const std::variant<std::uint8_t, RoomRejection>& result) {
        REQUIRE(std::holds_alternative<std::uint8_t>(result));
        return std::get<std::uint8_t>(result);
    };
    const auto rejection_of = [](const std::variant<std::uint8_t, RoomRejection>& result) {
        REQUIRE(std::holds_alternative<RoomRejection>(result));
        return std::get<RoomRejection>(result);
    };
    CHECK(rejection_of(create("", 2)) == RoomRejection::BadName);
    CHECK(rejection_of(create("a\nb", 2)) == RoomRejection::BadName);
    CHECK(rejection_of(create("Aces", 0)) == RoomRejection::BadSize);
    CHECK(rejection_of(create("Aces", 5)) == RoomRejection::BadSize);
    CHECK(room_of(create("Aces", 2)) == 0);

    // The logins fill the rooms in use, each up to its size, then open a new one.
    const Session first = accepted(table, "p0");
    const Session second = accepted(table, "p1");
    const Session third = accepted(table, "p2");
    CHECK(first.room == 0);
    CHECK(second.room == 0);
    CHECK(second.player == 1);
    CHECK(third.room == 1);
    CHECK(third.player == 0);
    CHECK(room_of(create("Bees", 1)) == 2);
    CHECK(rejection_of(create("Cees", 4)) == RoomRejection::NoRoomLeft);
    CHECK(accepted(table, "p3").room == 1);

    const auto waiting = static_cast<std::uint8_t>(RoomState::Waiting);
    CHECK(table.list_rooms() ==
          std::vector<RoomRecord>{
              {.room = 0, .name = "Aces", .players = 2, .max_players = 2, .state = waiting},
              {.room = 1, .name = "Room 2", .players = 2, .max_players = 4, .state = waiting},
              {.room = 2, .name = "Bees", .players = 0, .max_players = 1, .state = waiting}});

    // A bound player: its room plays. The last player of a room ends it.
    CHECK(table.bind(third.token, Endpoint::v4({192, 168, 1, 10}, 50000)) == BindResult::Bound);
    CHECK(table.list_rooms().at(1).state == static_cast<std::uint8_t>(RoomState::Playing));
    CHECK(table.end(first.token));
    CHECK(table.list_rooms().size() == 3);
    CHECK(table.end(second.token));
    CHECK(table.list_rooms().size() == 2);
    CHECK(room_of(create("Cees", 3)) == 0);

    SessionTable large{14};
    large.set_rooms(100);
    CHECK(large.rooms() == rtype::max_listed_rooms);
}

TEST_CASE("join, leave and start messages follow the RFC example", "[server][rooms]") {
    // RFC section 3.7, without the length prefixes.
    CHECK(rtype::write_join_room(1) == bytes({0x0A, 0x01}));
    CHECK(rtype::write_room_joined(1, 2) == bytes({0x0B, 0x01, 0x02}));
    CHECK(rtype::write_join_rejected(rtype::JoinRejection::RoomFull) == bytes({0x0C, 0x02}));
    CHECK(rtype::write_leave_room() == bytes({0x0D}));
    CHECK(rtype::write_set_ready(true) == bytes({0x0E, 0x01}));
    const rtype::RoomPlayers players{.room = 1,
                                     .players = {{.player = 0, .name = "R9", .ready = true},
                                                 {.player = 1, .name = "Kaz", .ready = false}}};
    const std::vector<std::byte> list = bytes({0x0F, 0x01, 0x02, 0x00, 0x02, 0x00, 0x52, 0x39, 0x01,
                                               0x01, 0x03, 0x00, 0x4B, 0x61, 0x7A, 0x00});
    CHECK(rtype::write_room_players(players) == list);
    CHECK(rtype::read_room_players(list) == players);
    CHECK(rtype::write_game_start() == bytes({0x10}));
}

TEST_CASE("join, leave and start messages are read whole or not at all", "[server][rooms]") {
    CHECK(rtype::read_join_room(bytes({0x0A, 0x03})) == std::uint8_t{3});
    CHECK(!rtype::read_join_room(bytes({0x0A})).has_value());
    CHECK(rtype::read_room_joined(bytes({0x0B, 0x01, 0x02})) ==
          std::pair<std::uint8_t, std::uint8_t>{1, 2});
    CHECK(!rtype::read_room_joined(bytes({0x0B, 0x01})).has_value());
    CHECK(rtype::read_join_rejected(bytes({0x0C, 0x09})) == std::uint8_t{9});
    CHECK(rtype::read_leave_room(bytes({0x0D})));
    CHECK(!rtype::read_leave_room(bytes({0x0D, 0x00})));
    CHECK(rtype::read_set_ready(bytes({0x0E, 0x00})) == false);
    CHECK(!rtype::read_set_ready(bytes({0x0E, 0x02})).has_value());
    CHECK(!rtype::read_set_ready(bytes({0x0E, 0x01, 0x00})).has_value());
    CHECK(rtype::read_game_start(bytes({0x10})));
    CHECK(!rtype::read_game_start(bytes({0x0F})));

    CHECK(rtype::read_room_players(bytes({0x0F, 0x02, 0x00})) ==
          rtype::RoomPlayers{.room = 2, .players = {}});
    CHECK(!rtype::read_room_players(bytes({0x0F, 0x02, 0x05})).has_value());
    CHECK(!rtype::read_room_players(bytes({0x0F, 0x02, 0x01, 0x00, 0x01, 0x00, 0x41, 0x02}))
               .has_value());
    CHECK(!rtype::read_room_players(bytes({0x0F, 0x02, 0x01, 0x00, 0x01, 0x00, 0x41})).has_value());
    CHECK_THROWS_AS(rtype::write_room_players(rtype::RoomPlayers{
                        .room = 0, .players = std::vector<rtype::RoomPlayer>(5)}),
                    std::invalid_argument);
}

TEST_CASE("sessions out of game join and leave rooms, and get ready", "[server][sessions][rooms]") {
    SessionTable table{17};
    table.set_rooms(2);
    const Session first = accepted(table, "a");
    const Session second = accepted(table, "b");
    REQUIRE(std::holds_alternative<std::uint8_t>(
        table.create_room({.name = "Aces", .max_players = 2})));
    const auto number = [](const std::variant<std::uint8_t, rtype::JoinRejection>& result) {
        REQUIRE(std::holds_alternative<std::uint8_t>(result));
        return std::get<std::uint8_t>(result);
    };
    const auto rejection = [](const std::variant<std::uint8_t, rtype::JoinRejection>& result) {
        REQUIRE(std::holds_alternative<rtype::JoinRejection>(result));
        return std::get<rtype::JoinRejection>(result);
    };

    // Into room 1: the lowest free number there; room 0 ends with its last session.
    CHECK(rejection(table.join(first.token, 5)) == rtype::JoinRejection::NoSuchRoom);
    CHECK(number(table.join(first.token, 1)) == 0);
    CHECK(number(table.join(first.token, 1)) == 0);
    CHECK(number(table.join(second.token, 1)) == 1);
    CHECK(table.list_rooms().size() == 1);
    const Session third = accepted(table, "c");
    CHECK(third.room == 0);
    CHECK(rejection(table.join(third.token, 1)) == rtype::JoinRejection::RoomFull);

    // Ready, then back to the lobby: not ready any more, and no game to bind to.
    CHECK(table.set_ready(first.token, true));
    CHECK(!table.set_ready(first.token, true));
    CHECK(table.members(1).at(0).ready);
    CHECK(table.leave(first.token) == 1U);
    CHECK(!table.leave(first.token).has_value());
    CHECK(table.find(first.token)->room == rtype::no_room);
    CHECK(!table.find(first.token)->ready);
    CHECK(!table.set_ready(first.token, true));
    const Endpoint home = Endpoint::v4({192, 168, 1, 10}, 50000);
    CHECK(table.bind(first.token, home) == BindResult::NoRoom);
    CHECK(table.members(1).size() == 1);

    // In game, a session stays in its room.
    CHECK(table.bind(second.token, home) == BindResult::Bound);
    CHECK(rejection(table.join(second.token, 0)) == rtype::JoinRejection::InGame);
    CHECK(!table.leave(second.token).has_value());
    CHECK(!table.set_ready(second.token, true));

    // From the lobby to a room.
    CHECK(number(table.join(first.token, 0)) == 1);
    CHECK(table.size() == 3);
}

TEST_CASE("the same seed gives the same tokens", "[server][sessions]") {
    SessionTable first{99};
    SessionTable second{99};
    CHECK(accepted(first, "a").token == accepted(second, "a").token);
}

TEST_CASE("the first Connect binds its endpoint to the session", "[server][sessions]") {
    SessionTable table{3};
    const Session first = accepted(table, "R9");
    const Session second = accepted(table, "R10");
    const Endpoint home = Endpoint::v4({192, 168, 1, 10}, 50000);
    const Endpoint other = Endpoint::v4({203, 0, 113, 7}, 50000);

    CHECK(table.find(home) == nullptr);
    CHECK(table.bind(0, home) == BindResult::UnknownSession);
    CHECK(table.bind(first.token + second.token, home) == BindResult::UnknownSession);
    CHECK(table.bind(first.token, home) == BindResult::Bound);
    REQUIRE(table.find(home) != nullptr);
    CHECK(table.find(home)->token == first.token);
    CHECK(table.find(first.token)->endpoint == home);

    // The Accept was lost: the same Connect binds nothing new and is answered again.
    CHECK(table.bind(first.token, home) == BindResult::AlreadyBound);
    // A third party with the token, or another session from the same endpoint: refused.
    CHECK(table.bind(first.token, other) == BindResult::Taken);
    CHECK(table.bind(second.token, home) == BindResult::Taken);
    CHECK(table.find(other) == nullptr);
    CHECK(table.bind(second.token, other) == BindResult::Bound);

    // Ending the session frees its endpoint.
    CHECK(table.end(first.token));
    CHECK(table.find(home) == nullptr);
    const Session third = accepted(table, "R11");
    CHECK(table.bind(third.token, home) == BindResult::Bound);
}

TEST_CASE("the client reads the answers to its login", "[client][login]") {
    const LoginAccepted accepted{.session = 0xDEADBEEF, .player = 2, .udp_port = 4242};
    CHECK(rtype::read_login_accepted(rtype::write_login_accepted(accepted)) ==
          std::optional{accepted});
    CHECK(
        !rtype::read_login_accepted(bytes({0x02, 0xEF, 0xBE, 0xAD, 0xDE, 0x02, 0x92})).has_value());
    CHECK(!rtype::read_login_accepted(rtype::write_login_rejected(LoginRejection::ServerFull))
               .has_value());

    CHECK(rtype::read_login_rejected(rtype::write_login_rejected(LoginRejection::NameTaken)) ==
          std::uint8_t{4});
    CHECK(rtype::read_login_rejected(bytes({0x03, 0x09})) == std::uint8_t{9});
    CHECK(!rtype::read_login_rejected(bytes({0x03})).has_value());
    CHECK(!rtype::read_login_rejected(bytes({0x03, 0x01, 0x00})).has_value());
}
