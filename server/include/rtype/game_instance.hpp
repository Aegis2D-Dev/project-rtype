/// @file
/// @brief The game run by the server, and the players of the sessions in it.
#pragma once
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/ecs/entity.hpp>
#include <aegis2d/ecs/waves.hpp>

#include <rtype/server_game.hpp>
#include <rtype/tuning.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace rtype {

/// @brief What the game says of a player, for the snapshots.
struct PlayerState {
    /// @brief Its ship, or `std::nullopt` once out of lives.
    std::optional<aegis2d::ecs::Entity> ship;
    /// @brief Lives left, 0 once out.
    std::uint32_t lives = 0;
    /// @brief Score; once out, the last score of its ship.
    std::uint32_t score = 0;
};

/// @brief The one game of the server, with up to tuning::max_players players.
/// @details A session joins when its UDP endpoint is bound (RFC section 3.4) and leaves
/// when it ends. Each player gets the ship of its player number, the `player` of
/// `LoginAccepted`, so the number picks its ship and spawn point. When the last player
/// leaves, the instance starts a fresh game with the same seed and waves, for the next
/// players. Not thread-safe: use it from one thread.
class GameInstance {
public:
    /// @brief Creates an empty game.
    /// @param seed Seed of the random spawns, or `std::nullopt` for none (see ServerGame).
    /// @param waves Waves to play in every game, at their start.
    GameInstance(std::optional<std::uint64_t> seed, aegis2d::ecs::WaveSchedule waves);

    /// @brief Adds the ship of a player to the game.
    /// @param player Player number of the session, below tuning::max_players.
    /// @return The ship.
    /// @throws std::out_of_range If @p player is not below tuning::max_players.
    /// @throws std::logic_error If the player is in the game already.
    aegis2d::ecs::Entity join(std::uint8_t player);

    /// @brief Removes a player from the game; its ship disappears.
    /// @param player Player number of the session.
    /// @return `false` if the player was not in the game.
    bool leave(std::uint8_t player);

    /// @brief Sets the actions a player holds, applied at every tick until the next call.
    /// @param player Player number of the session.
    /// @param input Held actions, from the player's most recent `Input`.
    /// @return `false` if the player is not in the game or is out of lives.
    bool set_input(std::uint8_t player, aegis2d::ecs::PlayerInput input);

    /// @brief Sets how many ticks back a player's missiles meet the enemies (lag
    /// compensation, `ServerGame::set_rewind()`).
    /// @param player Player number of the session.
    /// @param ticks rewind_ticks() of its round trip time.
    /// @return `false` if the player is not in the game or is out of lives.
    bool set_rewind(std::uint8_t player, std::uint32_t ticks);

    /// @brief Ship of a player.
    /// @param player Player number.
    /// @return The ship, or `std::nullopt` if the player is not in the game or is out.
    [[nodiscard]] std::optional<aegis2d::ecs::Entity> ship(std::uint8_t player) const;

    /// @brief State of a player.
    /// @details Remembers the score of each living ship, which the ship takes with it when it
    /// is destroyed, so a player out of lives keeps its score.
    /// @param player Player number.
    /// @return Its ship, lives and score, or `std::nullopt` if it is not in the game.
    [[nodiscard]] std::optional<PlayerState> player_state(std::uint8_t player);

    /// @brief Players in the game, out of lives or not.
    /// @return 0 to tuning::max_players.
    [[nodiscard]] std::size_t players() const noexcept;

    /// @brief Number of games played: 1, then 1 more each time the last player leaves.
    /// @return The count since the construction.
    [[nodiscard]] std::uint64_t games() const noexcept;

    /// @brief The game being played.
    /// @return The current game; another one after the last player leaves.
    [[nodiscard]] ServerGame& game() noexcept;

    /// @copydoc game()
    [[nodiscard]] const ServerGame& game() const noexcept;

private:
    void start();

    std::optional<std::uint64_t> seed_;
    aegis2d::ecs::WaveSchedule waves_;
    // ServerGame cannot move: each game is a new object.
    std::unique_ptr<ServerGame> game_;
    std::array<std::optional<aegis2d::ecs::Entity>, tuning::max_players> ships_;
    std::array<std::uint32_t, tuning::max_players> scores_{};
    std::uint64_t games_ = 0;
};

} // namespace rtype
