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
#include <iterator>
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
    std::uint32_t room = 0;
    while (room < listings_.size() && !has_place(room)) {
        ++room;
    }
    const bool new_room = room == listings_.size();
    if (new_room) {
        room = static_cast<std::uint32_t>(std::ranges::find_if(listings_, unused_room) -
                                          listings_.begin());
    }
    if (room == listings_.size() || sessions_.size() >= listings_.size() * tuning::max_players) {
        return LoginRejection::ServerFull;
    }
    if (std::ranges::any_of(sessions_,
                            [&](const Session& session) { return session.name == request.name; })) {
        return LoginRejection::NameTaken;
    }
    if (new_room) {
        listings_.at(room) =
            Listing{.name = std::format("Room {}", room + 1), .max_players = tuning::max_players};
    }
    return sessions_.emplace_back(Session{.token = new_token(),
                                          .room = room,
                                          .player = free_number(room),
                                          .name = request.name,
                                          .endpoint = std::nullopt,
                                          .ready = false});
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
    if (session->room == no_room) {
        return BindResult::NoRoom;
    }
    session->endpoint = endpoint;
    return BindResult::Bound;
}

bool SessionTable::end(std::uint32_t token) noexcept {
    const auto found = std::ranges::find(sessions_, token, &Session::token);
    if (found == sessions_.end()) {
        return false;
    }
    const std::uint32_t room = found->room;
    sessions_.erase(found);
    end_if_empty(room);
    return true;
}

std::variant<std::uint8_t, JoinRejection> SessionTable::join(std::uint32_t token,
                                                             std::uint32_t room) {
    Session* const session = find_mutable(token);
    if (session == nullptr || session->endpoint) {
        return JoinRejection::InGame;
    }
    if (room >= listings_.size() || !listings_.at(room)) {
        return JoinRejection::NoSuchRoom;
    }
    if (session->room == room) {
        return session->player;
    }
    if (!has_place(room)) {
        return JoinRejection::RoomFull;
    }
    const std::uint32_t old = session->room;
    session->player = free_number(room);
    session->room = room;
    session->ready = false;
    end_if_empty(old);
    return session->player;
}

std::optional<std::uint32_t> SessionTable::leave(std::uint32_t token) {
    Session* const session = find_mutable(token);
    if (session == nullptr || session->endpoint || session->room == no_room) {
        return std::nullopt;
    }
    const std::uint32_t old = session->room;
    session->room = no_room;
    session->ready = false;
    end_if_empty(old);
    return old;
}

bool SessionTable::set_ready(std::uint32_t token, bool ready) noexcept {
    Session* const session = find_mutable(token);
    if (session == nullptr || session->endpoint || session->room == no_room ||
        session->ready == ready) {
        return false;
    }
    session->ready = ready;
    return true;
}

std::vector<Session> SessionTable::members(std::uint32_t room) const {
    std::vector<Session> members;
    std::ranges::copy_if(sessions_, std::back_inserter(members),
                         [room](const Session& session) { return session.room == room; });
    std::ranges::sort(members, {}, &Session::player);
    return members;
}

const Session* SessionTable::find(std::uint32_t token) const noexcept {
    const auto found = std::ranges::find(sessions_, token, &Session::token);
    return found == sessions_.end() ? nullptr : &*found;
}

const Session* SessionTable::find(const aegis2d::net::Endpoint& endpoint) const noexcept {
    const auto found = std::ranges::find_if(
        sessions_, [&](const Session& session) { return session.endpoint == endpoint; });
    return found == sessions_.end() ? nullptr : &*found;
}

Session* SessionTable::find_mutable(std::uint32_t token) noexcept {
    const auto found = std::ranges::find(sessions_, token, &Session::token);
    return found == sessions_.end() ? nullptr : &*found;
}

std::size_t SessionTable::size() const noexcept {
    return sessions_.size();
}

void SessionTable::set_rooms(std::uint32_t rooms) {
    listings_.resize(std::clamp<std::size_t>(rooms, 1, max_listed_rooms));
}

std::uint32_t SessionTable::rooms() const noexcept {
    return static_cast<std::uint32_t>(listings_.size());
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
    for (std::uint32_t room = 0; room < listings_.size(); ++room) {
        const std::optional<Listing>& listing = listings_.at(room);
        if (!listing) {
            continue;
        }
        const bool playing = std::ranges::any_of(sessions_, [&](const Session& session) {
            return session.room == room && session.endpoint.has_value();
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

bool SessionTable::has_place(std::uint32_t room) const {
    const std::optional<Listing>& listing = listings_.at(room);
    return listing.has_value() && players_in(room) < listing->max_players;
}

std::size_t SessionTable::players_in(std::uint32_t room) const noexcept {
    return static_cast<std::size_t>(std::ranges::count(sessions_, room, &Session::room));
}

std::uint8_t SessionTable::free_number(std::uint32_t room) const noexcept {
    std::uint8_t player = 0;
    while (std::ranges::any_of(sessions_, [&](const Session& session) {
        return session.room == room && session.player == player;
    })) {
        ++player;
    }
    return player;
}

// A room with no session left is no longer in use.
void SessionTable::end_if_empty(std::uint32_t room) noexcept {
    if (room < listings_.size() && players_in(room) == 0) {
        listings_.at(room).reset();
    }
}

std::uint32_t SessionTable::new_token() noexcept {
    std::uint32_t token = 0;
    while (token == 0 || find(token) != nullptr) {
        token = random_.next();
    }
    return token;
}

} // namespace rtype
