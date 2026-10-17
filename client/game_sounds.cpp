#include "game_sounds.hpp"

#include <aegis2d/audio/audio.hpp>
#include <aegis2d/audio/event_sounds.hpp>
#include <aegis2d/core/event_bus.hpp>
#include <aegis2d/core/log.hpp>
#include <aegis2d/core/paths.hpp>

#include <rtype/events.hpp>
#include <rtype/snapshot.hpp>

namespace rtype {

namespace audio = aegis2d::audio;
using aegis2d::core::asset_path;

GameSounds::GameSounds(aegis2d::core::EventBus& events) {
    try {
        device_.emplace();
        effects_.emplace(*device_, events);
        effects_->play_on<SpawnEvent>(
            device_->load(asset_path("sounds/shot.wav")), [](const SpawnEvent& spawned) {
                return spawned.kind == static_cast<std::uint8_t>(EntityKind::PlayerMissile);
            });
        effects_->play_on<DestroyEvent>(device_->load(asset_path("sounds/explosion.wav")),
                                        [](const DestroyEvent& destroyed) {
                                            return destroyed.reason == DestroyReason::Killed;
                                        });
        effects_->play_on<DeathEvent>(device_->load(asset_path("sounds/death.wav")));
        music_ = device_->load(asset_path("sounds/music.wav"));
        device_->play_music(*music_);
    } catch (const audio::AudioError& error) {
        aegis2d::core::log_warn("no sound: {}", error.what());
        effects_.reset();
        device_.reset();
        music_.reset();
    }
}

bool GameSounds::available() const noexcept {
    return device_.has_value();
}

void GameSounds::toggle_music() {
    if (!device_.has_value() || !music_.has_value()) {
        return;
    }
    if (device_->music_playing()) {
        device_->stop_music();
    } else {
        device_->play_music(*music_);
    }
}

bool GameSounds::music_playing() const noexcept {
    return device_.has_value() && device_->music_playing();
}

void GameSounds::update() noexcept {
    if (device_.has_value()) {
        device_->update();
    }
}

} // namespace rtype
