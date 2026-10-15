/// @file
/// @brief Sessions of the server: who is logged in, under which token and player number.
#pragma once
#include <aegis2d/core/random.hpp>

#include <rtype/login.hpp>
#include <rtype/tuning.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace rtype {

/// @brief A logged-in player (RFC section 3).
struct Session {
    /// @brief Token given in `LoginAccepted`, never 0.
    std::uint32_t token = 0;
    /// @brief Player number, 0 to tuning::max_players - 1.
    std::uint8_t player = 0;
    /// @brief Name given at login, valid as valid_player_name() says.
    std::string name;
};

/// @brief The sessions of the game, one per player slot.
/// @details Decides each login (version, name, free slot), draws the tokens and frees the
/// slot when a session ends. It does not touch sockets, so it is tested alone. Not
/// thread-safe: use it from the network thread only.
class SessionTable {
public:
    /// @brief Creates an empty table drawing tokens from the system's random source.
    SessionTable();

    /// @brief Creates an empty table drawing tokens from a seed, for tests.
    /// @param seed Seed of the tokens: the same seed gives the same tokens.
    explicit SessionTable(std::uint64_t seed) noexcept;

    /// @brief Decides a login and opens its session if it is accepted.
    /// @details Checked in this order: the version, the name, a free slot, the name not
    /// used by another player. The session takes the lowest free player number and a new
    /// random token, never 0 and never the token of another session.
    /// @param request Version and name received.
    /// @return The new session, or why the login is refused.
    [[nodiscard]] std::variant<Session, LoginRejection> login(const LoginRequest& request);

    /// @brief Ends a session and frees its slot and name.
    /// @param token Token of the session.
    /// @return `true` if the session existed.
    bool end(std::uint32_t token) noexcept;

    /// @brief Finds a session.
    /// @param token Token received.
    /// @return The session, or `nullptr` if no session has this token.
    [[nodiscard]] const Session* find(std::uint32_t token) const noexcept;

    /// @brief Number of open sessions.
    /// @return 0 to tuning::max_players.
    [[nodiscard]] std::size_t size() const noexcept;

private:
    [[nodiscard]] std::uint32_t new_token() noexcept;

    aegis2d::core::Random random_;
    std::array<std::optional<Session>, tuning::max_players> slots_;
};

} // namespace rtype
