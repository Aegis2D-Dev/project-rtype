# Add an enemy type

This guide adds a new enemy from start to finish: its sprite, its values, its creation
function, its name in the waves files, a wave that uses it, its tests and its
documentation. It follows a real example, the **Pata-Pata**: a small, fast enemy that
flutters up and down in short waves and does not fire. Replace `pata_pata` and `pata_` with
your enemy's name as you go.

Everything happens in this repository, in one pull request: an enemy is game content made
of engine components. If your enemy needs a behavior no engine function provides yet
(homing, a laser, splitting in two...), write that behavior in the engine first (see
[Add a system](how-to.md#add-a-system)), get it merged, then come back here.

At the end, you will have touched these files:

| Step | File |
|---|---|
| 1. Sprite | `assets/sprites/pata_pata.png` |
| 2. Values | `game/include/rtype/tuning.hpp` |
| 3. Creation | `game/include/rtype/entities.hpp`, `game/entities.cpp` |
| 4. Name for the waves | `game/include/rtype/waves.hpp`, `game/entities.cpp` |
| 5. Waves | `assets/waves/stage1.json` (or your own waves file) |
| 6. Tests | `tests/entities_test.cpp`, `tests/waves_test.cpp` |
| 7. Documentation | `docs/gameplay.md`, `CHANGELOG.md`, `changelogs/` |

## 1. The sprite

Draw the enemy as a PNG in `assets/sprites/`, named after its type:
`assets/sprites/pata_pata.png`. For an animation, put the frames side by side, all the
same size. Note its size on screen in logical pixels (the play area is 1280 × 720); it is
also the base of its hit box.

!!! note "What the server sends, what the client draws"
    The server simulates the game and only knows sizes: an enemy's `ecs::Sprite` holds its
    `size` and keeps `texture` at 0, because texture ids belong to each client's GPU. The
    client finds the image by the type name with the engine's `render::TextureManager`,
    e.g. `textures.load("pata_pata", core::asset_path("sprites/pata_pata.png"))`; keeping
    the file name equal to the type name is what lets it do so. The client does not draw
    the world yet (it comes with the network tickets): until then, the image is not
    loaded and the server tests are what checks your enemy.

## 2. Its values

Every number goes to `game/include/rtype/tuning.hpp`, next to the Bydos values, with a
comment saying what it is. The Pata-Pata is faster and smaller than a Bydos slave, worth
more points, and flutters in short, quick waves:

```cpp
/// @brief Speed of a Pata-Pata, to the left.
inline constexpr float pata_speed = 200.0F;
/// @brief Size of a Pata-Pata.
inline constexpr aegis2d::Vec2 pata_size{24.0F, 24.0F};
/// @brief Health points of a Pata-Pata: one missile kills it.
inline constexpr std::int32_t pata_health = 1;
/// @brief Points for killing a Pata-Pata.
inline constexpr std::uint32_t pata_score = 200;
/// @brief Largest distance of a Pata-Pata above or below its line as it flutters.
inline constexpr float pata_wave_amplitude = 32.0F;
/// @brief Seconds for a Pata-Pata to flutter up, down and back to its line.
inline constexpr float pata_wave_period = 0.8F;
```

An enemy whose path leaves the screen must come back within `despawn_margin` (64 px), or
it is destroyed as gone: keep its wave amplitude at or below it. Add a check next to the
Bydos one, at the end of the file:

```cpp
static_assert(pata_wave_amplitude <= despawn_margin,
              "a Pata-Pata fluttering off screen must not be despawned before it comes back");
```

## 3. Its creation function

An enemy is an entity with components. Pick them from this table; the first group is
needed by every enemy:

| Component | Why | Value |
|---|---|---|
| `Transform` | Where it is | the spawn position |
| `Velocity` | It flies left | `{-tuning::pata_speed, 0}` |
| `Sprite` | Its size on screen | `{.size = tuning::pata_size}` |
| `Collider` | Hit by player missiles, hits players | `.layer = layer::enemy, .mask = mask::enemy` |
| `Health` | Missiles can kill it | `tuning::pata_health` |
| `ScoreValue` | Its killer gets points | `tuning::pata_score` |
| `OffscreenDespawn` | Destroyed once it has crossed the screen | `tuning::despawn_margin` |
| `NetworkId` | Clients can name it | `NetworkId::of(entity)` |
| `Damage` (optional) | Ramming it costs the player a life | `{.amount = tuning::ram_damage, .consumed_on_hit = false}` |
| `SineMotion` (optional) | It weaves around its line | amplitude and period |
| `Weapon` + `AutoTrigger` (optional) | It fires on its own | cooldown, first shot; see `spawn_bydos_slave()` |

Declare the function in `game/include/rtype/entities.hpp`, documented like the others:

```cpp
/// @brief Creates a Pata-Pata, a small enemy that flutters quickly and does not fire.
/// @details It flies left at tuning::pata_speed while fluttering around its line
/// (tuning::pata_wave_amplitude, every tuning::pata_wave_period seconds), dies from one
/// missile, gives tuning::pata_score points to its killer, rams players and is destroyed
/// once it has crossed the screen; it can be created beyond the right edge.
/// @param registry World of the game.
/// @param position Center of the Pata-Pata, on its line.
/// @return The Pata-Pata.
aegis2d::ecs::Entity spawn_pata_pata(aegis2d::ecs::Registry& registry, aegis2d::Vec2 position);
```

Then write it in `game/entities.cpp`, after `spawn_bydos_slave()`:

```cpp
ecs::Entity spawn_pata_pata(ecs::Registry& registry, Vec2 position) {
    const ecs::Entity pata = registry.create();
    registry.emplace<ecs::Transform>(pata, position);
    registry.emplace<ecs::Velocity>(pata, Vec2{-tuning::pata_speed, 0.0F});
    registry.emplace<ecs::SineMotion>(
        pata, ecs::SineMotion{.amplitude = Vec2{0.0F, tuning::pata_wave_amplitude},
                              .period = tuning::pata_wave_period});
    registry.emplace<ecs::Sprite>(pata, ecs::Sprite{.size = tuning::pata_size});
    registry.emplace<ecs::Collider>(
        pata, ecs::Collider{.size = tuning::pata_size, .layer = layer::enemy, .mask = mask::enemy});
    registry.emplace<ecs::Health>(pata, tuning::pata_health);
    registry.emplace<ecs::Damage>(
        pata, ecs::Damage{.amount = tuning::ram_damage, .consumed_on_hit = false});
    registry.emplace<ecs::ScoreValue>(pata, tuning::pata_score);
    registry.emplace<ecs::OffscreenDespawn>(pata, tuning::despawn_margin);
    registry.emplace<ecs::NetworkId>(pata, ecs::NetworkId::of(pata));
    return pata;
}
```

No system needs to change: the server's systems work on components, not on enemy types.
The `movement` and `sine_motion` systems move it, `collision`, `damage` and `score` make
it die and pay, `despawn` removes it, and `enemy_fire` would make it fire if it had a
`Weapon` and an `AutoTrigger`.

## 4. Its name in the waves files

Waves files name enemies by type. Add the name to `rtype::enemy_types` in
`game/include/rtype/waves.hpp`:

```cpp
inline constexpr std::array enemy_types{std::string_view{"bydos_slave"},
                                        std::string_view{"pata_pata"}};
```

and its branch to `spawn_enemy()` in `game/entities.cpp`:

```cpp
    if (type == "pata_pata") {
        return spawn_pata_pata(registry, position);
    }
```

A name in `enemy_types` without its branch in `spawn_enemy()` fails the test *every enemy
type a waves file can name can be spawned*; a name in a waves file that is not in
`enemy_types` stops the server at start.

## 5. A wave that uses it

Now the enemy can appear without touching the code again. Add a wave to
`assets/waves/stage1.json` (the format is in [Add a wave](how-to.md#add-a-wave)), for
instance a quick file of Pata-Pata after the V:

```json
{
    // A swarm of Pata-Pata in the middle of the screen.
    "delay": 3.0,
    "spawns": [
        {"type": "pata_pata", "position": [1296, 360], "count": 8, "every": 0.3}
    ]
}
```

Keep the lines between `amplitude + half height` and `720 - amplitude - half height`
(here 44 and 676) so that the enemy flutters on screen. Then build and let the server
check the file:

```bash
cmake --build build/linux-debug
./build/linux-debug/r-type_server
```

```text
waves: .../assets/waves/stage1.json (5 waves, 37 enemies in 24.4 s, looping)
```

A mistake is reported with its place, e.g.
`waves.4.spawns.0.type: unknown type "pata" (known: bydos_slave, pata_pata)`.
To try a wave without changing the stage, put it in its own file and run
`./build/linux-debug/r-type_server --waves my_waves.json`.

## 6. Tests

Test what makes your enemy different, reading every value from `rtype::tuning`. In
`tests/entities_test.cpp`, its components:

```cpp
TEST_CASE("a Pata-Pata flutters left, fast, and does not fire", "[rtype][entities]") {
    Registry registry;
    const Entity pata = rtype::spawn_pata_pata(registry, Vec2{1296.0F, 360.0F});
    CHECK(registry.get<Velocity>(pata).value == Vec2{-tuning::pata_speed, 0.0F});
    CHECK(registry.get<SineMotion>(pata).period == tuning::pata_wave_period);
    CHECK(registry.get<ScoreValue>(pata).points == tuning::pata_score);
    CHECK(registry.get<Collider>(pata).layer == layer::enemy);
    CHECK(registry.has<OffscreenDespawn>(pata));
    CHECK(!registry.has<Weapon>(pata));
}
```

and in `tests/waves_test.cpp`, that a waves file brings it into a game and that a missile
kills it for its points (a `ServerGame` with a player, a wave written by the test):

```cpp
TEST_CASE("a Pata-Pata from a waves file can be shot down", "[rtype][waves]") {
    const auto file = write_waves("pata.json", R"({"waves": [{"spawns": [
        {"type": "pata_pata", "position": [700, 144]}
    ]}]})");
    ServerGame game;
    const Entity ship = game.add_player();
    game.play_waves(rtype::load_waves(file));
    game.tick(step_from_rate(tuning::tick_rate));
    REQUIRE(game.spawned().size() == 1);
    const Entity pata = game.spawned().front();
    // Hold it on the ship's line, so that the missile meets it.
    game.registry().get<SineMotion>(pata).amplitude = Vec2{};
    game.registry().get<Transform>(pata).position.y =
        game.registry().get<Transform>(ship).position.y;
    rtype::spawn_player_missile(game.registry(), ship);
    for (int tick = 0; tick < 60 && game.registry().alive(pata); ++tick) {
        game.tick(step_from_rate(tuning::tick_rate));
    }
    CHECK(!game.registry().alive(pata));
    CHECK(game.registry().get<Score>(ship).points == tuning::pata_score);
    std::filesystem::remove(file);
}
```

Add the `using` lines the new names need at the top of each file (`Velocity`,
`SineMotion`, `Score`...). The tests of `stage1.json` keep checking that every enemy of the
file starts beyond the right edge and within the Bydos lanes; if your enemy needs other
lanes, adapt that test to your type. Run everything:

```bash
ctest --test-dir build/linux-debug --output-on-failure
tools/format.sh && tools/lint.sh build/linux-debug
```

## 7. Documentation and pull request

- `docs/gameplay.md`: a row in the [entities table](gameplay.md#entities) (function and
  components), its values in the [tuning table](gameplay.md#tuning), and a word in
  [Enemy waves](gameplay.md#enemy-waves) if it changes what the waves contain.
- A changelog file in `changelogs/` and its row in `CHANGELOG.md` (see
  [CONTRIBUTING](https://github.com/Aegis2D-Dev/project-rtype/blob/develop/CONTRIBUTING.md)).
- One pull request, in three commits: the code and assets, the tests, then the
  documentation and changelogs.

## Checklist

- [ ] `assets/sprites/<type>.png`, named after the type
- [ ] Its values in `tuning.hpp`, and a `static_assert` if it weaves
- [ ] `spawn_<type>()` declared, documented and written, with `OffscreenDespawn` and
      `NetworkId`
- [ ] Its name in `enemy_types` and its branch in `spawn_enemy()`
- [ ] A wave that uses it, checked by `r-type_server`
- [ ] Tests of its components and of its behavior in a game
- [ ] Entities and tuning tables, changelog
