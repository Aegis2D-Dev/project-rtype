# How-to guides

Short recipes for the changes made most often. Every change comes with tests in `tests/`
and a changelog entry (see
[CONTRIBUTING.md](https://github.com/Aegis2D-Dev/project-rtype/blob/develop/CONTRIBUTING.md)).

## Change a gameplay value

Every number lives in `game/include/rtype/tuning.hpp`. Change the constant there and
nowhere else:

```cpp
/// @brief Seconds between two shots of a player holding fire (five shots a second).
inline constexpr float player_fire_cooldown = 0.2F;
```

Update the [tuning table](gameplay.md#tuning) and run the tests: tests that check a value
read it from `rtype::tuning`, so only behavior that really changed fails.

## Add an enemy type

1. Add its values to `tuning.hpp` (speed, size, health, score).
2. Declare its creation function in `game/include/rtype/entities.hpp`, documented like the
   others, and write it in `game/entities.cpp`. Give it the `enemy` layer and mask so it
   hits players and is hit by their missiles, and a `NetworkId` so clients can name it:

    ```cpp
    ecs::Entity spawn_bydos_turret(ecs::Registry& registry, Vec2 position) {
        const ecs::Entity turret = registry.create();
        registry.emplace<ecs::Transform>(turret, position);
        registry.emplace<ecs::Sprite>(turret, ecs::Sprite{.size = tuning::turret_size});
        registry.emplace<ecs::Collider>(
            turret,
            ecs::Collider{.size = tuning::turret_size, .layer = layer::enemy, .mask = mask::enemy});
        registry.emplace<ecs::Health>(turret, tuning::turret_health);
        registry.emplace<ecs::ScoreValue>(turret, tuning::turret_score);
        registry.emplace<ecs::NetworkId>(turret, ecs::NetworkId::of(turret));
        return turret;
    }
    ```

3. Test its components in `tests/entities_test.cpp`, and its behavior through a
   `ServerGame` in `tests/server_game_test.cpp` (for instance: one missile kills it and
   gives its points to the shooter).
4. Add it to the [entities table](gameplay.md#entities).

## Add a system

1. If the behavior is generic (any shoot'em up would need it), write it in the engine
   first, as a function of `aegis2d::ecs` taking the `Registry` and the `Tick`, with its
   tests; once it is merged in the engine's `develop`, wire it here.
2. Register it in `ServerGame::register_systems()` (`game/server_game.cpp`), at its place
   in the tick:

    ```cpp
    systems_.add("waves", [this](ecs::Registry& world, const ecs::Tick& tick) {
        waves_.update(world, tick, spawned_);    // the enemies of the current wave
    });
    ```

3. If it produces events the clients need, keep them in a member of `ServerGame` cleared
   at each tick and expose them with a `const` getter, like `fired()`.
4. Update the system list in the `ServerGame` comment (`server_game.hpp`), the
   [tick table](gameplay.md#one-tick), and the test that checks the system order.

## Write a test

Tests are Catch2 test cases in `tests/`, built into `rtype_tests`; tag them `[rtype]` plus
the area (`[server]`...) and the ticket number. A gameplay test drives a `ServerGame` tick
by tick, with the same step as the server, and reads its values from `rtype::tuning`:

```cpp
TEST_CASE("a player holding right stops at the right edge", "[rtype][server]") {
    rtype::ServerGame game;
    const auto player = game.add_player();
    aegis2d::ecs::PlayerInput input;
    input.set(aegis2d::ecs::Action::Right, true);
    game.set_input(player, input);

    for (std::uint32_t tick = 0; tick < 10 * rtype::tuning::tick_rate; ++tick) {
        game.tick(aegis2d::core::step_from_rate(rtype::tuning::tick_rate));
    }
    const float edge = rtype::tuning::area_width - (rtype::tuning::player_size.x / 2.0F);
    CHECK(game.registry().get<aegis2d::ecs::Transform>(player).position.x ==
          Catch::Approx(edge));
}
```

Run one test or one tag:

```bash
./build/linux-debug/tests/rtype_tests "[server]"
ctest --test-dir build/linux-debug -R "right edge"
```

## Update the documentation

Pages are in `docs/` and listed in `mkdocs.yml`; the API reference comes from the
`///` comments of `game/include/`. Check both before pushing, as CI does:

```bash
pip install -r docs/requirements.txt
mkdocs build --strict
doxygen docs/Doxyfile
```
