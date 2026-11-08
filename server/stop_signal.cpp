#include <rtype/stop_signal.hpp>

#include <atomic>
#include <csignal>
#include <cstdlib>

namespace rtype {

namespace {

// Lock-free, so setting it from a signal handler is safe. Global: a signal handler can only
// reach globals.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> stopping{false};
static_assert(std::atomic<bool>::is_always_lock_free);

extern "C" void on_stop_signal(int /*signal*/) {
    if (stopping.exchange(true)) {
        std::_Exit(forced_exit_status);
    }
}

} // namespace

bool install_stop_handlers() {
    const bool interrupt = std::signal(SIGINT, on_stop_signal) != SIG_ERR;
    const bool terminate = std::signal(SIGTERM, on_stop_signal) != SIG_ERR;
    return interrupt && terminate;
}

bool stop_requested() noexcept {
    return stopping.load();
}

void request_stop() noexcept {
    stopping.store(true);
}

} // namespace rtype
