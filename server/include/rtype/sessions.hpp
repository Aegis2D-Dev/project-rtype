/// @file
/// @brief Sessions of the server: who is logged in, under which token and player number.
#pragma once
#include <aegis2d/core/random.hpp>
#include <aegis2d/net/endpoint.hpp>

#include <rtype/login.hpp>
#include <rtype/rooms.hpp>
#include <rtype/tuning.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rtype {

/// @brief Room of a session in the lobby, out of every room (`LeaveRoom`, RFC section 3.7).
inline constexpr std::uint32_t no_room = 0xFFFFFFFFU;

/// @brief A logged-in player (RFC section 3).
struct Session {
    /// @brief Token given in `LoginAccepted`, never 0.
    std::uint32_t token = 0;
    /// @brief Room the player is in, 0 to SessionTable::rooms() - 1, or no_room in the lobby.
    std::uint32_t room = 0;
    /// @brief Player number in its room, 0 to tuning::max_players - 1.
    std::uint8_t player = 0;
    /// @brief Name given at login, valid as valid_player_name() says.
    std::string name;
    /// @brief UDP endpoint the player plays from, once its `Connect` arrived (RFC 3.4): the
    /// player is then in game.
    std::optional<aegis2d::net::Endpoint> endpoint;
    /// @brief Whether the player is ready to start the game of its room (`SetReady`).
    bool ready = false;
};

/// @brief What SessionTable::bind() did with a `Connect`.
enum class BindResult : std::uint8_t {
    /// @brief The endpoint is now bound to the session: answer with `Accept`.
    Bound,
    /// @brief The endpoint was already bound to this session (the `Accept` was lost):
    /// answer with `Accept` again.
    AlreadyBound,
    /// @brief No session has this token: drop the datagram.
    UnknownSession,
    /// @brief The session is bound to another endpoint, or the endpoint to another
    /// session: drop the datagram, so a third party who learned a token cannot take over
    /// the session.
    Taken,
    /// @brief The session is in the lobby, with no game to join: drop the datagram.
    NoRoom,
};

/// @brief The sessions of the server and the rooms they are in.
/// @details Decides each login (version, name, free place), puts it in a room, draws the
/// tokens, moves the sessions between the rooms and the lobby, binds each session to the UDP
/// endpoint of its `Connect`, and forgets it when it ends. It also keeps the rooms in use,
/// with their name and size (RFC section 3.6): a room is in use from its creation, by
/// create_room() or for a login that found no free place, until its last session leaves or
/// ends. It does not touch sockets, so it is tested alone. Not thread-safe: use it from the
/// network thread only.
class SessionTable {
public:
    /// @brief Creates an empty table drawing tokens from the system's random source.
    SessionTable();

    /// @brief Creates an empty table drawing tokens from a seed, for tests.
    /// @param seed Seed of the tokens: the same seed gives the same tokens.
    explicit SessionTable(std::uint64_t seed) noexcept;

    /// @brief Decides a login and opens its session if it is accepted.
    /// @details Checked in this order: the version, the name, a free place, the name not
    /// used by another player of any room. The session goes to the first room in use with a
    /// free place, so a room fills before the next gets players, or else to a new room for
    /// tuning::max_players named `Room <number + 1>`; the server takes rooms() times
    /// tuning::max_players sessions, those of the lobby included. It takes the lowest free
    /// player number of its room, and a new random token, never 0 and never the token of
    /// another session.
    /// @param request Version and name received.
    /// @return The new session, or why the login is refused.
    [[nodiscard]] std::variant<Session, LoginRejection> login(const LoginRequest& request);

    /// @brief Binds the source endpoint of a `Connect` to the session of its token.
    /// @param token The `session` field of the datagram.
    /// @param endpoint Source address and port of the datagram.
    /// @return What was done, and so whether to answer with `Accept`.
    BindResult bind(std::uint32_t token, const aegis2d::net::Endpoint& endpoint);

    /// @brief Ends a session and frees its place, name and endpoint; the last one of its room
    /// ends the room.
    /// @param token Token of the session.
    /// @return `true` if the session existed.
    bool end(std::uint32_t token) noexcept;

    /// @brief Moves a session out of game to another room in use (`JoinRoom`, RFC 3.7).
    /// @details Checked in this order: the session not in game, the room in use, a free
    /// place in it. The session takes the lowest free number there and is not ready; its old
    /// room ends if it was its last session. Joining its own room changes nothing.
    /// @param token Token of the session.
    /// @param room Number of the room.
    /// @return Its number in the room, or why it did not join.
    [[nodiscard]] std::variant<std::uint8_t, JoinRejection> join(std::uint32_t token,
                                                                 std::uint32_t room);

    /// @brief Moves a session out of game from its room to the lobby (`LeaveRoom`).
    /// @param token Token of the session.
    /// @return The room it left, or `std::nullopt` if it is in game, in the lobby already, or
    /// unknown.
    std::optional<std::uint32_t> leave(std::uint32_t token);

    /// @brief Sets whether a session out of game, in a room, is ready (`SetReady`).
    /// @param token Token of the session.
    /// @param ready Its new state.
    /// @return `true` if it changed.
    bool set_ready(std::uint32_t token, bool ready) noexcept;

    /// @brief The sessions of a room.
    /// @param room Number of the room.
    /// @return Copies of them, by player number.
    [[nodiscard]] std::vector<Session> members(std::uint32_t room) const;

    /// @brief Finds a session.
    /// @param token Token received.
    /// @return The session, or `nullptr` if no session has this token.
    [[nodiscard]] const Session* find(std::uint32_t token) const noexcept;

    /// @brief Finds the session bound to an endpoint.
    /// @param endpoint Source address and port of a datagram.
    /// @return The session, or `nullptr` if the endpoint is not bound.
    [[nodiscard]] const Session* find(const aegis2d::net::Endpoint& endpoint) const noexcept;

    /// @brief Number of open sessions, those of the lobby included.
    /// @return 0 to rooms() times tuning::max_players.
    [[nodiscard]] std::size_t size() const noexcept;

    /// @brief Sets how many rooms the sessions are spread over, tuning::max_players in each
    /// at most.
    /// @details Call it before the first login: the server is full once every room is.
    /// @param rooms Number of rooms, 1 to max_listed_rooms; 1 by default.
    void set_rooms(std::uint32_t rooms);

    /// @brief Number of rooms the sessions are spread over.
    /// @return 1 unless set_rooms() said otherwise.
    [[nodiscard]] std::uint32_t rooms() const noexcept;

    /// @brief Creates a room, empty, for the next logins (`CreateRoom`, RFC section 3.6).
    /// @details Checked in this order: the name, by the rules of player names
    /// (valid_player_name()), the size, the lives, the difficulty, the waves (empty, or one of
    /// set_waves()), the friendly fire, a room not in use. The room takes the lowest number of
    /// those.
    /// @param request Name and most players of the room.
    /// @return Its number, or why it is not created.
    [[nodiscard]] std::variant<std::uint8_t, RoomRejection>
    create_room(const CreateRoomRequest& request);

    /// @brief Sets the waves files a room may be created with, besides the server's own.
    /// @param names Their names, without `.json` (waves_names()).
    void set_waves(std::vector<std::string> names);

    /// @brief The rules of a room in use.
    /// @details A room opened for a login has the default RoomRules.
    /// @param room Number of the room.
    /// @return Its rules, or `std::nullopt` if it is not in use.
    [[nodiscard]] std::optional<RoomRules> rules(std::uint32_t room) const;

    /// @brief The rooms in use, by number, as a `RoomList` describes them.
    /// @details A room is RoomState::Playing once one of its sessions is bound.
    /// @return One record per room in use.
    [[nodiscard]] std::vector<RoomRecord> list_rooms() const;

private:
    // A room in use.
    struct Listing {
        std::string name;
        std::uint8_t max_players = tuning::max_players;
        RoomRules rules;
    };

    [[nodiscard]] bool has_place(std::uint32_t room) const;
    [[nodiscard]] std::size_t players_in(std::uint32_t room) const noexcept;
    [[nodiscard]] std::uint8_t free_number(std::uint32_t room) const noexcept;
    void end_if_empty(std::uint32_t room) noexcept;
    [[nodiscard]] std::uint32_t new_token() noexcept;
    [[nodiscard]] Session* find_mutable(std::uint32_t token) noexcept;

    aegis2d::core::Random random_;
    std::vector<Session> sessions_;
    // The rooms in use, by number.
    std::vector<std::optional<Listing>> listings_ = std::vector<std::optional<Listing>>(1);
    std::vector<std::string> waves_;
};

} // namespace rtype
