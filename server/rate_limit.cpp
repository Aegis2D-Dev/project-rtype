#include <aegis2d/net/endpoint.hpp>

#include <rtype/rate_limit.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

net::Endpoint address_of(const net::Endpoint& endpoint) noexcept {
    const auto bytes = endpoint.address_bytes();
    if (endpoint.is_v6()) {
        std::array<std::uint8_t, 16> address{};
        std::ranges::copy(bytes.first(address.size()), address.begin());
        return net::Endpoint::v6(address, 0);
    }
    std::array<std::uint8_t, 4> address{};
    std::ranges::copy(bytes.first(address.size()), address.begin());
    return net::Endpoint::v4(address, 0);
}

RateLimiter::RateLimiter(std::uint32_t per_second) noexcept : per_second_{per_second} {}

bool RateLimiter::allow(const net::Endpoint& source, Clock::time_point now) {
    Window& counted = windows_[address_of(source)];
    if (counted.count == 0 || now - counted.start >= window) {
        counted = Window{.start = now, .count = 0};
    }
    if (counted.count >= per_second_) {
        return false;
    }
    ++counted.count;
    return true;
}

void RateLimiter::prune(Clock::time_point now) {
    std::erase_if(windows_,
                  [now](const auto& entry) { return now - entry.second.start >= window; });
}

std::size_t RateLimiter::addresses() const noexcept {
    return windows_.size();
}

bool DropReport::count(const net::Endpoint& source, std::string_view reason,
                       Clock::time_point now) {
    const auto [found, added] = sources_.try_emplace(address_of(source));
    if (added) {
        found->second.since = now;
    }
    return ++found->second.reasons[reason] == 1;
}

std::vector<std::string> DropReport::take_due(Clock::time_point now) {
    std::vector<std::string> lines;
    for (auto entry = sources_.begin(); entry != sources_.end();) {
        const auto& [address, source] = *entry;
        if (now - source.since < report_period) {
            ++entry;
            continue;
        }
        std::uint64_t total = 0;
        std::string reasons;
        for (const auto& [reason, count] : source.reasons) {
            total += count;
            std::format_to(std::back_inserter(reasons), "{}{} {}", reasons.empty() ? "" : ", ",
                           count, reason);
        }
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - source.since);
        lines.push_back(std::format("{}: {} dropped in {} s ({})", address.address(), total,
                                    seconds.count(), reasons));
        entry = sources_.erase(entry);
    }
    return lines;
}

} // namespace rtype
