#include <aegis2d/core/random.hpp>
#include <aegis2d/net/endpoint.hpp>

#include <rtype/login.hpp>
#include <rtype/rooms.hpp>
#include <rtype/sessions.hpp>
#include <rtype/tuning.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <random>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

namespace {

// A room number no room uses.
constexpr auto unused_room = [](const auto& listing) { return !listing.has_value(); };

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
    // The first room in use with a free place, or else the first room not in use.
    std::size_t room = 0;
    while (room < listings_.size() && !has_place(room)) {
        ++room;
    }
    const bool new_room = room == listings_.size();
    if (new_room) {
        room = static_cast<std::size_t>(std::ranges::find_if(listings_, unused_room) -
                                        listings_.begin());
        if (room == listings_.size()) {
            return LoginRejection::ServerFull;
        }
    }
    if (std::ranges::any_of(slots_, [&](const std::optional<Session>& slot) {
            return slot && slot->name == request.name;
        })) {
        return LoginRejection::NameTaken;
    }
    if (new_room) {
        listings_.at(room) =
            Listing{.name = std::format("Room {}", room + 1), .max_players = tuning::max_players};
    }
    const auto first = slots_.begin() + static_cast<std::ptrdiff_t>(room * tuning::max_players);
    const auto free =
        std::ranges::find_if(first, first + tuning::max_players,
                             [](const std::optional<Session>& slot) { return !slot.has_value(); });
    *free = Session{.token = new_token(),
                    .room = static_cast<std::uint32_t>(room),
                    .player = static_cast<std::uint8_t>(free - first),
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
            const std::size_t room = slot->room;
            slot.reset();
            if (players_in(room) == 0) {
                listings_.at(room).reset();
            }
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
    const std::size_t count = std::clamp<std::size_t>(rooms, 1, max_listed_rooms);
    slots_.resize(count * tuning::max_players);
    listings_.resize(count);
}

std::variant<std::uint8_t, RoomRejection>
SessionTable::create_room(const CreateRoomRequest& request) {
    if (!valid_player_name(request.name)) {
        return RoomRejection::BadName;
    }
    if (request.max_players < 1 || request.max_players > tuning::max_players) {
        return RoomRejection::BadSize;
    }
    const auto unused = std::ranges::find_if(listings_, unused_room);
    if (unused == listings_.end()) {
        return RoomRejection::NoRoomLeft;
    }
    *unused = Listing{.name = request.name, .max_players = request.max_players};
    return static_cast<std::uint8_t>(unused - listings_.begin());
}

std::vector<RoomRecord> SessionTable::list_rooms() const {
    std::vector<RoomRecord> rooms;
    for (std::size_t room = 0; room < listings_.size(); ++room) {
        const std::optional<Listing>& listing = listings_.at(room);
        if (!listing) {
            continue;
        }
        const bool playing = std::ranges::any_of(slots_, [&](const std::optional<Session>& slot) {
            return slot && slot->room == room && slot->endpoint.has_value();
        });
        rooms.push_back(RoomRecord{
            .room = static_cast<std::uint8_t>(room),
            .name = listing->name,
            .players = static_cast<std::uint8_t>(players_in(room)),
            .max_players = listing->max_players,
            .state = static_cast<std::uint8_t>(playing ? RoomState::Playing : RoomState::Waiting)});
    }
    return rooms;
}

bool SessionTable::has_place(std::size_t room) const {
    const std::optional<Listing>& listing = listings_.at(room);
    return listing.has_value() && players_in(room) < listing->max_players;
}

std::size_t SessionTable::players_in(std::size_t room) const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(
        slots_, [room](const std::optional<Session>& slot) { return slot && slot->room == room; }));
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
