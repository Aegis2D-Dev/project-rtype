#include <catch2/catch_test_macros.hpp>
#include <rtype/stop_signal.hpp>

#include <csignal>

// One signal only: a second one would end the test process, as it ends the server.
TEST_CASE("SIGTERM asks the server to stop", "[server][shutdown]") {
    REQUIRE(rtype::install_stop_handlers());
    CHECK(!rtype::stop_requested());
    REQUIRE(std::raise(SIGTERM) == 0);
    CHECK(rtype::stop_requested());
}
