# Add an enemy type

This guide adds a new enemy from start to finish: its sprite, its script, a wave that uses
it, and its tests. It follows a real example, the **Pata-Pata**: a small, fast enemy that
flutters up and down in short waves and does not fire. Replace `pata_pata` with your
enemy's name as you go.

An enemy type is game content, not code: its values and its behavior are a Lua script that
each game of the server loads when it starts, so a new type needs **no recompilation**. If your enemy
needs something scripts cannot do yet (a component the engine does not give scripts, such
as a laser), write it in the engine first (see [Add a system](how-to.md#add-a-system)),
get it merged, then come back here.

At the end, you will have touched these files:

| Step | File |
|---|---|
| 1. Sprite | `assets/sprites/pata_pata.png` |
| 2. Script | `assets/enemies/pata_pata.lua` |
| 3. Waves | `assets/waves/stage1.json` (or your own waves file) |
| 4. Tests | `tests/enemy_scripts_test.cpp`, or a test of your own |
| 5. Documentation | `docs/gameplay.md`, `CHANGELOG.md`, `changelogs/` |

## 1. The sprite

Draw the enemy as a PNG in `assets/sprites/`, named after its type:
`assets/sprites/pata_pata.png`. For an animation, put the frames side by side, all the
same size. Note its size on screen in logical pixels (the play area is 1280 × 720); it is
also its hit box.

!!! note "What the server sends, what the client draws"
    The server simulates the game and only knows sizes: an enemy's `ecs::Sprite` holds its
    `size` and keeps `texture` at 0, because texture ids belong to each client's GPU. The
    protocol has one kind of enemy for now (`EntityKind::BydosSlave`, RFC section 4.1):
    clients draw every enemy with the Bydos sprite, at the Bydos size, until the protocol
    tells them each enemy's type. The sprite named after the type is what they will load
    then, with the engine's `render::TextureManager`.

## 2. Its script

Create `assets/enemies/pata_pata.lua`. Its file name is the type's name, as the waves files
write it: lowercase letters, digits and `_`. The file describes the type with
`enemy { ... }`:

```lua
-- Pata-Pata: a small, fast enemy that flutters up and down in short waves and does not
-- fire.

local speed = 200      -- to the left, in logical pixels per second
local amplitude = 32   -- above and below its line; at most 64 (see below)
local period = 0.8     -- seconds to flutter up, down and back to its line

enemy {
    width = 24, height = 24,
    health = 1,
    score = 200,

    spawn = function(self)
        self.line = self.y
        ecs.get(self.entity, "Velocity").x = -speed
    end,

    update = function(self, dt)
        local offset = amplitude * math.sin(2 * math.pi * self.age / period)
        ecs.get(self.entity, "Transform").y = self.line + offset
    end,
}
```

The values, all optional:

| Field | Default | What it is |
|---|---|---|
| `width`, `height` | 32, 32 | Size on screen and hit box |
| `health` | 1 | Health points: 1 dies from one missile |
| `score` | 100 | Points for its killer |
| `fire_every` | 0 | Seconds between two shots to the left; 0 never fires |
| `first_shot` | 1 | Seconds before its first shot, so that it fires once on screen |

The game gives every enemy what all enemies need: a Transform where it appears, a Velocity
of zero, its Sprite and Collider on the enemy layer (player missiles hit it, it rams
players), its Health, the points of its killer, a despawn once it has crossed the screen, a
network id, and a weapon that fires on its own if `fire_every` is above 0. The script does
the rest:

- `spawn(self)` runs once, when the enemy appears: give it its speed there;
- `update(self, dt)` runs at every tick, after the `movement` system: move it, turn it,
  change its speed.

`self` is the enemy's own table: `self.entity` is its id for the `ecs` functions,
`self.x` and `self.y` where it appeared, `self.age` its seconds alive, and `self.random` a
number from 0 to 1 the game draws for the random spawns (0 in the waves), to start
enemies spawned together at different points of their pattern. The script may keep its
own fields there, as `self.line` above. The components and their fields are in the
engine's [scripting guide](https://github.com/Aegis2D-Dev/aegis2d/blob/develop/docs/scripting.md):
`Transform.x`, `Velocity.y`, `Sprite.width`...

An enemy whose path leaves the screen must come back within 64 px (`despawn_margin`), or it
is destroyed as gone: keep its wave amplitude at or below it.

Scripts run sandboxed: no files, no `require`, a budget of instructions and memory. A
mistake in a script stops the server at start, with its file and line; here, the comma
after the `end` of `spawn` is missing:

```text
r-type_server: .../assets/enemies/pata_pata.lua:18: '}' expected (to close '{' at line 8) near 'update'
```

An error while the game runs (a field that does not exist, a nil value), or a behavior that
runs too long (an endless loop), is logged once, with its line, and stops that behavior: a
failed `update` leaves the enemies of the type at their speed, a failed `spawn` means no
more enemies of the type. The game goes on.

## 3. A wave that uses it

Add a wave to `assets/waves/stage1.json` (the format is in [Add a
wave](how-to.md#add-a-wave)), for instance a quick file of Pata-Pata after the V:

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
(here 44 and 676) so that the enemy flutters on screen. Then build, which only copies the
new files next to the server, and let the server check them (the build copies, it never
deletes: after renaming or removing a script, delete its old copy in
`build/<preset>/assets/enemies` and `build/<preset>/tests/assets/enemies`):

```bash
cmake --build build/linux-debug
./build/linux-debug/r-type_server
```

```text
waves: .../assets/waves/stage1.json (5 waves, 37 enemies in 24.4 s, looping)
enemies: bydos_slave pata_pata (.../assets/enemies)
```

A waves file naming a type that has no script is reported with its place, e.g.
`waves.4.spawns.0.type: unknown type "pata" (known: bydos_slave, pata_pata)`. To try a
wave without changing the stage, put it in its own file and run
`./build/linux-debug/r-type_server --waves my_waves.json`.

## 4. Tests

Test what makes your enemy different. A `ServerGame` loads the scripts of `assets/enemies`
(copied next to the tests), so a test spawns your enemy by its name:

```cpp
TEST_CASE("a Pata-Pata flutters left, fast, and does not fire", "[rtype][enemies]") {
    rtype::ServerGame game;
    const Entity pata = game.spawn_enemy("pata_pata", Vec2{1296.0F, 360.0F});
    CHECK(game.registry().get<Velocity>(pata).value == Vec2{-200.0F, 0.0F});
    CHECK(game.registry().get<aegis2d::ecs::ScoreValue>(pata).points == 200);
    CHECK(!game.registry().has<aegis2d::shmup::Weapon>(pata));
    for (int tick = 0; tick < 12; ++tick) {  // 0.2 s: a quarter of its flutter, at its lowest
        game.tick(step_from_rate(tuning::tick_rate));
    }
    CHECK(game.registry().get<Transform>(pata).position.y == Approx(392.0F).margin(1.0));
}
```

`tests/enemy_scripts_test.cpp` shows more: a type written by the test in a folder of its
own (`ServerGame game{std::nullopt, folder}`), played by a waves file. The test *every
enemy type a waves file can name can be spawned* checks every script of `assets/enemies`.
Run everything:

```bash
ctest --test-dir build/linux-debug --output-on-failure
```

## 5. Documentation and pull request

- `docs/gameplay.md`: its values in the [tuning table](gameplay.md#tuning), and a word in
  [Enemy waves](gameplay.md#enemy-waves) if it changes what the waves contain.
- A changelog file in `changelogs/` and its row in `CHANGELOG.md` (see
  [CONTRIBUTING](https://github.com/Aegis2D-Dev/project-rtype/blob/develop/CONTRIBUTING.md)).
- One pull request: the sprite, the script and the waves, then the tests, then the
  documentation and changelogs.

## Checklist

- [ ] `assets/sprites/<type>.png`, named after the type
- [ ] `assets/enemies/<type>.lua`, with `enemy { ... }`, its values and its behavior
- [ ] A wave amplitude of at most 64 px if it weaves
- [ ] A wave that uses it, checked by `r-type_server`
- [ ] Tests of its values and of its behavior in a game
- [ ] Tuning table, changelog
