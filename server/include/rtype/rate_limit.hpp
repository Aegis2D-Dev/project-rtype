/// @file
/// @brief Limits on what one address may send, and the summary of what was dropped (RFC 6.6, 6.7).
#pragma once
#include <aegis2d/net/endpoint.hpp>

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rtype {

/// @brief Limits on the traffic of one IP address.
struct RateLimits {
    /// @brief Datagrams accepted from one address per second, its ports together.
    /// @details 4 players behind one address send about 4 × 70 datagrams per second (an
    /// `Input` per tick, plus `Heartbeat` and `Event` datagrams).
    std::uint32_t datagrams_per_second = 300;
    /// @brief TCP connections open at once from one address.
    std::uint32_t connections_per_address = 8;
};

/// @brief The address of an endpoint, its port set to 0: the key of the limits.
/// @param endpoint Source of a datagram or a connection.
/// @return The same address with port 0.
[[nodiscard]] aegis2d::net::Endpoint address_of(const aegis2d::net::Endpoint& endpoint) noexcept;

/// @brief Counts the datagrams of each address in windows of one second.
/// @details The first datagrams_per_second datagrams of a window pass; the next ones are
/// refused until the window ends. Addresses silent for a whole window are forgotten by
/// prune(). Not thread-safe: use it from the network thread.
class RateLimiter {
public:
    /// @brief Clock of the windows.
    using Clock = std::chrono::steady_clock;

    /// @brief Length of a window.
    static constexpr std::chrono::seconds window{1};

    /// @brief Creates a limiter with no address yet.
    /// @param per_second Datagrams accepted per address and window; 0 refuses everything.
    explicit RateLimiter(std::uint32_t per_second) noexcept;

    /// @brief Counts a datagram and tells whether it may be handled.
    /// @param source Source of the datagram; only its address counts.
    /// @param now Current time.
    /// @return `false` if the address already sent its datagrams of the window.
    [[nodiscard]] bool allow(const aegis2d::net::Endpoint& source, Clock::time_point now);

    /// @brief Forgets the addresses whose window ended, to bound the memory a flood of
    /// forged addresses can take.
    /// @param now Current time.
    void prune(Clock::time_point now);

    /// @brief Number of addresses being counted.
    /// @return The count.
    [[nodiscard]] std::size_t addresses() const noexcept;

private:
    struct Window {
        Clock::time_point start;
        std::uint32_t count = 0;
    };

    std::uint32_t per_second_;
    std::unordered_map<aegis2d::net::Endpoint, Window> windows_;
};

/// @brief The chat rate of one session (RFC section 3.8): a token bucket.
/// @details A session sends burst messages at once, then gets one more back every refill,
/// up to burst kept. A message refused does not count. Not thread-safe.
class ChatRate {
public:
    /// @brief Clock of the refills.
    using Clock = std::chrono::steady_clock;
    /// @brief Messages a session may send at once.
    static constexpr std::uint32_t burst = 5;
    /// @brief Time for one message to come back.
    static constexpr std::chrono::seconds refill{1};

    /// @brief Counts a message and tells whether it may be relayed.
    /// @param now Current time, never earlier than the previous call's.
    /// @return `false` if the session is over the rate: the message is a flood.
    [[nodiscard]] bool allow(Clock::time_point now) noexcept;

private:
    std::uint32_t tokens_ = burst;
    // When the last token came back, or the first message with every token left.
    std::optional<Clock::time_point> since_;
};

/// @brief Counts the datagrams dropped from each address, by reason, for a summary in the
/// log at most every report_period per address (RFC section 6.7): a flood never becomes one
/// log line per datagram.
class DropReport {
public:
    /// @brief Clock of the reports.
    using Clock = std::chrono::steady_clock;

    /// @brief Shortest time between two summaries of one address.
    static constexpr std::chrono::seconds report_period{10};

    /// @brief Counts a dropped datagram.
    /// @param source Source of the datagram; only its address counts.
    /// @param reason Why it was dropped; must outlive the report (a literal).
    /// @param now Current time.
    /// @return `true` the first time this reason is counted for the address since its last
    /// summary, to log an offender once.
    bool count(const aegis2d::net::Endpoint& source, std::string_view reason,
               Clock::time_point now);

    /// @brief Takes the summaries due: one line per address whose first drop since its last
    /// summary is at least report_period old, its counts then forgotten.
    /// @param now Current time.
    /// @return Lines such as `"203.0.113.7: 1200 dropped in 10 s (1150 rate limit, 50
    /// endpoint not bound)"`, by address.
    [[nodiscard]] std::vector<std::string> take_due(Clock::time_point now);

private:
    struct Source {
        Clock::time_point since;
        std::map<std::string_view, std::uint64_t> reasons;
    };

    std::unordered_map<aegis2d::net::Endpoint, Source> sources_;
};

} // namespace rtype
