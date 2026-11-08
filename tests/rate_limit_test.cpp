#include <aegis2d/net/endpoint.hpp>

#include <catch2/catch_test_macros.hpp>
#include <rtype/rate_limit.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

using aegis2d::net::Endpoint;
using rtype::DropReport;
using rtype::RateLimiter;
using namespace std::chrono_literals;

TEST_CASE("an address gets its datagrams of each second, its ports together", "[server][rate]") {
    RateLimiter limiter{3};
    const auto start = RateLimiter::Clock::now();
    const Endpoint player = Endpoint::v4({203, 0, 113, 7}, 50000);
    const Endpoint same_address = Endpoint::v4({203, 0, 113, 7}, 50001);
    const Endpoint other = Endpoint::v4({198, 51, 100, 1}, 50000);

    CHECK(limiter.allow(player, start));
    CHECK(limiter.allow(same_address, start + 100ms));
    CHECK(limiter.allow(player, start + 200ms));
    CHECK(!limiter.allow(player, start + 300ms));
    CHECK(!limiter.allow(same_address, start + 999ms));
    CHECK(limiter.allow(other, start + 300ms));

    // A new window starts a second after the first datagram of the last one.
    CHECK(limiter.allow(player, start + 1s));
    CHECK(limiter.addresses() == 2);
    limiter.prune(start + 1s + 300ms);
    CHECK(limiter.addresses() == 1);
    limiter.prune(start + 2s);
    CHECK(limiter.addresses() == 0);

    RateLimiter closed{0};
    CHECK(!closed.allow(player, start));
}

TEST_CASE("addresses are compared without their port", "[server][rate]") {
    CHECK(rtype::address_of(Endpoint::v4({10, 0, 0, 1}, 4242)) == Endpoint::v4({10, 0, 0, 1}, 0));
    const Endpoint v6 = Endpoint::loopback(1);
    CHECK(rtype::address_of(v6).port() == 0);
    CHECK(rtype::address_of(v6).address() == v6.address());
}

TEST_CASE("dropped datagrams are summed up every 10 seconds per address", "[server][rate]") {
    DropReport report;
    const auto start = DropReport::Clock::now();
    const Endpoint flooder = Endpoint::v4({203, 0, 113, 7}, 50000);
    CHECK(report.count(flooder, "rate limit", start));
    CHECK(!report.count(Endpoint::v4({203, 0, 113, 7}, 50001), "rate limit", start + 1s));
    CHECK(report.count(flooder, "bad magic", start + 2s));
    CHECK(report.count(Endpoint::v4({198, 51, 100, 1}, 1), "bad magic", start + 5s));

    CHECK(report.take_due(start + 9s).empty());
    CHECK(report.take_due(start + 10s) ==
          std::vector<std::string>{"203.0.113.7: 3 dropped in 10 s (1 bad magic, 2 rate limit)"});
    CHECK(report.take_due(start + 10s).empty());
    CHECK(report.take_due(start + 15s) ==
          std::vector<std::string>{"198.51.100.1: 1 dropped in 10 s (1 bad magic)"});

    // Counted again from the next drop.
    CHECK(report.count(flooder, "rate limit", start + 20s));
}

TEST_CASE("a session chats 5 messages at once, then 1 more per second", "[server][rate][chat]") {
    using Clock = rtype::ChatRate::Clock;
    rtype::ChatRate rate;
    const Clock::time_point start{};
    for (std::uint32_t message = 0; message < rtype::ChatRate::burst; ++message) {
        CHECK(rate.allow(start));
    }
    // The sixth is a flood, and a refused message does not count.
    CHECK(!rate.allow(start));
    CHECK(!rate.allow(start + std::chrono::milliseconds{999}));
    CHECK(rate.allow(start + std::chrono::seconds{1}));
    CHECK(!rate.allow(start + std::chrono::milliseconds{1500}));
    CHECK(rate.allow(start + std::chrono::seconds{2}));

    // After a long silence, 5 at once again, not more.
    const Clock::time_point later = start + std::chrono::minutes{1};
    for (std::uint32_t message = 0; message < rtype::ChatRate::burst; ++message) {
        CHECK(rate.allow(later));
    }
    CHECK(!rate.allow(later));
}
