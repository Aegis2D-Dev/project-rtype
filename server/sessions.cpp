#include <aegis2d/core/random.hpp>
#include <aegis2d/net/endpoint.hpp>

#include <rtype/login.hpp>
#include <rtype/sessions.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

namespace {

// Two draws of the system's source: tokens must not be guessed from the start time.
std::uint64_t system_seed() {
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32U) | device();
}

} // namespace

SessionTable::SessionTable() : SessionTable{system_seed()} {}

SessionTable::SessionTable(std::uint64_t seed) noexcept : random_{seed} {}

std::variant<Session, LoginRejection> SessionTable::login(const LoginRequest& request) {
    if (request.version != protocol_version) {
        return LoginRejection::BadVersion;
    }
    if (!valid_player_name(request.name)) {
        return LoginRejection::BadName;
    }
    const auto free = std::ranges::find_if(
        slots_, [](const std::optional<Session>& slot) { return !slot.has_value(); });
    if (free == slots_.end()) {
        return LoginRejection::ServerFull;
    }
    if (std::ranges::any_of(slots_, [&](const std::optional<Session>& slot) {
            return slot && slot->name == request.name;
        })) {
        return LoginRejection::NameTaken;
    }
    const auto index = static_cast<std::size_t>(free - slots_.begin());
    *free = Session{.token = new_token(),
                    .room = static_cast<std::uint32_t>(index / tuning::max_players),
                    .player = static_cast<std::uint8_t>(index % tuning::max_players),
                    .name = request.name,
                    .endpoint = std::nullopt};
    return **free;
}

BindResult SessionTable::bind(std::uint32_t token, const aegis2d::net::Endpoint& endpoint) {
    Session* const session = find_mutable(token);
    if (session == nullptr) {
        return BindResult::UnknownSession;
    }
    if (session->endpoint == endpoint) {
        return BindResult::AlreadyBound;
    }
    if (session->endpoint || find(endpoint) != nullptr) {
        return BindResult::Taken;
    }
    session->endpoint = endpoint;
    return BindResult::Bound;
}

bool SessionTable::end(std::uint32_t token) noexcept {
    for (std::optional<Session>& slot : slots_) {
        if (slot && slot->token == token) {
            slot.reset();
            return true;
        }
    }
    return false;
}

const Session* SessionTable::find(std::uint32_t token) const noexcept {
    for (const std::optional<Session>& slot : slots_) {
        if (slot && slot->token == token) {
            return &*slot;
        }
    }
    return nullptr;
}

const Session* SessionTable::find(const aegis2d::net::Endpoint& endpoint) const noexcept {
    for (const std::optional<Session>& slot : slots_) {
        if (slot && slot->endpoint == endpoint) {
            return &*slot;
        }
    }
    return nullptr;
}

Session* SessionTable::find_mutable(std::uint32_t token) noexcept {
    for (std::optional<Session>& slot : slots_) {
        if (slot && slot->token == token) {
            return &*slot;
        }
    }
    return nullptr;
}

std::size_t SessionTable::size() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(
        slots_, [](const std::optional<Session>& slot) { return slot.has_value(); }));
}

void SessionTable::set_rooms(std::uint32_t rooms) {
    slots_.resize(static_cast<std::size_t>(std::max<std::uint32_t>(rooms, 1)) *
                  tuning::max_players);
}

std::uint32_t SessionTable::rooms() const noexcept {
    return static_cast<std::uint32_t>(slots_.size() / tuning::max_players);
}

std::uint32_t SessionTable::new_token() noexcept {
    std::uint32_t token = 0;
    while (token == 0 || find(token) != nullptr) {
        token = random_.next();
    }
    return token;
}

} // namespace rtype
