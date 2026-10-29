/// @file
/// @brief A player without a window: joins a server and holds random inputs.
#pragma once
#include <aegis2d/ecs/components.hpp>
#include <aegis2d/net/context.hpp>

#include <rtype/client_session.hpp>

#include <chrono>
#include <cstdint>
#include <random>

namespace rtype {

/// @brief Random actions held for random times, like a player mashing the keys.
/// @details Each pick holds a direction (or none) on each axis, and Fire most of the time,
/// for a time drawn between the shortest and longest hold. The same seed gives the same
/// inputs at the same times.
class RandomInputs {
public:
    /// @brief Clock of the holds.
    using Clock = std::chrono::steady_clock;
    /// @brief Shortest time an input is held.
    static constexpr std::chrono::milliseconds shortest_hold{150};
    /// @brief Longest time an input is held.
    static constexpr std::chrono::milliseconds longest_hold{900};

    /// @brief Creates the generator.
    /// @param seed Seed of the draws.
    explicit RandomInputs(std::uint32_t seed);

    /// @brief The input to hold now: a new one once the last has been held long enough.
    /// @param now Current time; the first call picks the first input.
    /// @return The held actions.
    [[nodiscard]] aegis2d::ecs::PlayerInput update(Clock::time_point now);

    /// @brief Number of inputs picked.
    /// @return The count since the generator was created.
    [[nodiscard]] std::uint64_t picks() const noexcept;

private:
    std::mt19937 random_;
    aegis2d::ecs::PlayerInput input_;
    Clock::time_point until_{};
    std::uint64_t picks_ = 0;
};

/// @brief What a bot needs to join a server.
struct BotSettings {
    /// @brief Server and player name.
    ClientSettings server{.host = "127.0.0.1", .tcp_port = 4242, .name = "bot"};
    /// @brief Seed of its inputs.
    std::uint32_t seed = 0;
};

/// @brief A client without window nor sound: a ClientSession whose inputs are RandomInputs.
/// @details start() joins the server as any client does (TCP login, then UDP binding); once
/// in game, update() sends the random inputs, one `Input` per server tick, which also keep
/// the session alive. leave() says goodbye with `Logout`. Same threading rules as
/// ClientSession: use it on the thread running the context.
class Bot {
public:
    /// @brief Clock of the session and the inputs.
    using Clock = std::chrono::steady_clock;

    /// @brief Creates a bot, not started.
    /// @param context Context that runs its sockets; it must outlive this object.
    /// @param settings Server, name and seed.
    Bot(aegis2d::net::NetworkContext& context, const BotSettings& settings);

    /// @brief Connects to the server and logs in.
    void start();

    /// @brief Runs the session's timers and, in game, picks the inputs.
    /// @param now Current time.
    void update(Clock::time_point now);

    /// @brief Leaves the server: sends `Logout`; keep running the context a little for it
    /// to leave.
    void leave();

    /// @brief The session with the server: its state, error, traffic and world.
    /// @return The session.
    [[nodiscard]] const ClientSession& session() const noexcept;

    /// @brief The actions held now.
    /// @return The last input given to the session.
    [[nodiscard]] aegis2d::ecs::PlayerInput input() const noexcept;

    /// @brief The generator of the inputs.
    /// @return It, for the number of inputs picked.
    [[nodiscard]] const RandomInputs& inputs() const noexcept;

private:
    ClientSession session_;
    RandomInputs inputs_;
    aegis2d::ecs::PlayerInput input_;
};

} // namespace rtype
