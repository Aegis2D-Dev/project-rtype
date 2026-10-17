/// @file
/// @brief The sounds of the game, played from the events the server sends.
#pragma once
#include <aegis2d/audio/audio.hpp>
#include <aegis2d/audio/event_sounds.hpp>
#include <aegis2d/core/event_bus.hpp>

#include <optional>

namespace rtype {

/// @brief Plays the game's sounds from the events published on a bus.
/// @details The client publishes each event of the server (rtype::SpawnEvent,
/// rtype::DestroyEvent, rtype::DeathEvent...) on the bus; this object says once which
/// sound each one plays: a player missile spawned fires, a kill explodes, a lost life plays
/// its own sound. The music loops behind. Without an audio output (a machine without sound
/// card, a CI container), the game goes on silently: available() is `false` and every call
/// does nothing.
class GameSounds {
public:
    /// @brief Opens the audio output and loads `assets/sounds/`.
    /// @param events Bus the client publishes the events on; it must outlive this object.
    explicit GameSounds(aegis2d::core::EventBus& events);

    /// @brief Whether the game has sound.
    /// @return `false` if the audio output or a sound could not be opened.
    [[nodiscard]] bool available() const noexcept;

    /// @brief Starts or stops the music.
    void toggle_music();

    /// @brief Whether the music plays.
    /// @return `true` while it loops.
    [[nodiscard]] bool music_playing() const noexcept;

    /// @brief Frees the sounds that ended; call it once a frame.
    void update() noexcept;

private:
    std::optional<aegis2d::audio::AudioDevice> device_;
    std::optional<aegis2d::audio::EventSounds> effects_;
    std::optional<aegis2d::audio::SoundId> music_;
};

} // namespace rtype
