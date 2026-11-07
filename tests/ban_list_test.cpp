#include <aegis2d/net/endpoint.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/ban_list.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

using aegis2d::net::Endpoint;
using rtype::BanList;

namespace {

// A file of a fresh temporary folder, for the bans of one test.
std::filesystem::path bans_file(std::string_view test) {
    const auto folder = std::filesystem::temp_directory_path() / "rtype_ban_list_test" / test;
    std::filesystem::remove_all(folder);
    return folder / "bans.json";
}

Endpoint address(std::string_view text, std::uint16_t port) {
    return Endpoint::parse(text, port).value_or(Endpoint{});
}

} // namespace

TEST_CASE("the bans survive a restart, whatever the port", "[server][bans]") {
    const std::filesystem::path file = bans_file("restart");
    {
        BanList bans{file};
        CHECK(bans.addresses().empty());
        CHECK(!bans.load_error().has_value());
        CHECK(bans.ban(address("203.0.113.7", 50000)));
        CHECK(bans.ban(address("2001:db8::1", 4242)));
        // Banned already, from another port: nothing changes.
        CHECK(!bans.ban(address("203.0.113.7", 1)));
        CHECK(bans.addresses().size() == 2);
    }
    CHECK(!std::filesystem::exists(file.string() + ".tmp"));
    const BanList restarted{file};
    CHECK(!restarted.load_error().has_value());
    CHECK(restarted.banned(address("203.0.113.7", 61000)));
    CHECK(restarted.banned(address("2001:db8::1", 0)));
    CHECK(!restarted.banned(address("203.0.113.8", 61000)));
    CHECK(!restarted.banned(Endpoint::loopback(4242)));
}

TEST_CASE("a corrupted ban list is reported, not fatal", "[server][bans]") {
    const std::filesystem::path file = bans_file("corrupted");
    for (const std::string_view text :
         {std::string_view{"{ not json"}, std::string_view{R"({"bans": 3})"},
          std::string_view{R"({"bans": ["203.0.113.7", "example.com"]})"},
          std::string_view{R"({"bans": [7]})"}}) {
        CAPTURE(text);
        std::filesystem::create_directories(file.parent_path());
        std::ofstream{file} << text;
        BanList bans{file};
        CHECK(bans.load_error().has_value());
        CHECK(bans.addresses().empty());
        // The next ban replaces the file with a valid list.
        CHECK(bans.ban(address("198.51.100.2", 0)));
        const BanList reread{file};
        CHECK(!reread.load_error().has_value());
        CHECK(reread.addresses().size() == 1);
    }
}
