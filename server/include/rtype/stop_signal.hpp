/// @file
/// @brief Stopping `r-type_server` with Ctrl+C (SIGINT) or SIGTERM.
#pragma once

namespace rtype {

/// @brief Exit status of a server stopped by a second signal, before its clean stop ended.
inline constexpr int forced_exit_status = 130;

/// @brief Handles SIGINT and SIGTERM: the first one asks the server to stop cleanly
/// (stop_requested() becomes `true`), a second one ends the process at once with
/// forced_exit_status.
/// @details The handler only sets a lock-free atomic flag, or calls `std::_Exit()`: nothing
/// else is safe in a signal handler. Windows has no SIGTERM from the system, but Ctrl+C
/// raises SIGINT.
/// @return `false` if a handler could not be installed: that signal then ends the process.
[[nodiscard]] bool install_stop_handlers();

/// @brief Tells whether a stop was asked by a signal. Thread-safe.
/// @return `true` once SIGINT or SIGTERM was received.
[[nodiscard]] bool stop_requested() noexcept;

} // namespace rtype
