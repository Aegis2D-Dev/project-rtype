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

## Add a wave

Waves are data: edit `assets/waves/stage1.json` (or a copy given to `r-type_server
--waves`), no recompilation needed. A wave waits `delay` seconds after the last enemy of the
previous wave, then spawns its groups:

```json
{
    "delay": 3.0,                                   // seconds after the previous wave
    "spawns": [
        // Four Bydos, one every 0.5 s, each 80 px lower, starting 1 s into the wave.
        {"type": "bydos_slave", "position": [1296, 160], "at": 1.0, "count": 4,
         "every": 0.5, "step": [0, 80]}
    ]
}
```

| Field | Default | Meaning |
|---|---|---|
| `type` | required | Enemy type, a script of `assets/enemies` ([add one](add-an-enemy.md)) |
| `position` | required | `[x, y]` of the first enemy; x = 1296 is just beyond the right edge |
| `at` | 0 | Seconds from the start of the wave |
| `count` | 1 | Enemies in the group |
| `every` | 0 | Seconds between two enemies of the group |
| `step` | `[0, 0]` | Offset between two enemies of the group |

Keep `y` between 64 and 656 (`tuning::bydos_lane_top` and `bydos_lane_bottom`) so a Bydos
weaves without leaving the screen; the test of `tests/waves_test.cpp` checks it for
`stage1.json`. Run `./build/linux-debug/r-type_server` (or with `--waves <file>`) to check a
file: it prints the number of waves and enemies, or the place of the first mistake.

## Add an enemy type

A new enemy is a sprite, a Lua script and a wave that uses it, with no recompilation.
[Add an enemy type](add-an-enemy.md) walks through all of it with a complete example.

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
by tick, with the same step as the server, and reads its values from `rtype::tuning`. A
`ServerGame` created without a seed spawns no enemy, so the test controls the whole world;
give it a seed (`rtype::ServerGame game{42};`) to test the random spawns, which are then
the same at every run:

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
