#include <aegis2d/net/endpoint.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/login.hpp>
#include <rtype/sessions.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using aegis2d::net::Endpoint;
using rtype::BindResult;
using rtype::LoginAccepted;
using rtype::LoginRejection;
using rtype::LoginRequest;
using rtype::LogoutReason;
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
    CHECK(!rtype::kind_of(bytes({0x05})).has_value());

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
