# Gameplay

Everything below runs on the server, in `rtype::ServerGame`. Positions are in logical
pixels of a 1280 × 720 play area, origin at the top-left corner, x to the right, y down;
speeds are per second, so the game plays the same at any tick rate.

## The server game

`ServerGame` holds the world (the engine's `Registry`), its systems in order, up to four
player slots, and the seed of its random enemy spawns.

```cpp
rtype::ServerGame game{seed};                    // same seed, same enemies (log it)
game.play_waves(rtype::load_waves(rtype::default_waves_file()));   // the stage's waves
const auto player = game.add_player();          // ship in the first free slot

aegis2d::ecs::PlayerInput input;
input.set(aegis2d::ecs::Action::Right, true);
input.set(aegis2d::ecs::Action::Fire, true);
game.set_input(player, input);                   // latest input, kept until the next one

game.tick(aegis2d::core::step_from_rate(rtype::tuning::tick_rate));
for (const auto missile : game.fired()) { /* send a spawn */ }
for (const auto missile : game.enemy_fired()) { /* send a spawn */ }
for (const auto bydos : game.spawned()) { /* send a spawn */ }
for (const auto& kill : game.damage().kills) { /* send a kill */ }
for (const auto& award : game.awards()) { /* send a score */ }
for (const auto& lost : game.lives_lost()) { /* send a death, or "out" */ }
for (const auto gone : game.despawned()) { /* send a removal */ }
if (game.game_over()) { /* every player who joined is out */ }
```

| Call | What it does |
|---|---|
| `ServerGame{seed}` | A game whose Bydos spawn at random from `seed`; `ServerGame{}` spawns none (tests) |
| `play_waves(waves)`, `waves()` | Plays the enemy waves of a file read by `load_waves()`; the schedule being played |
| `add_player()` | Spawns a ship in the first free slot; throws `std::length_error` with four players |
| `slot_of(player)` | Slot of a player, or nothing once it is out |
| `set_input(player, input)` | Held actions of a player; `false` for a player who is out (late packet) |
| `tick(step)` | Runs every system once, in order |
| `fired()`, `enemy_fired()`, `spawned()`, `damage()`, `awards()`, `lives_lost()`, `despawned()` | Events of the last tick, for the network layer |
| `seed()` | Seed of the random spawns, if any |
| `players()`, `game_over()` | Players with lives left; `true` once all of them are out |

A slot frees up when its player is out of lives, so a new player can join a running game.

## One tick

The systems run in this order (`game.systems().names()` lists them):

| # | System | Engine function | What happens |
|---|---|---|---|
| 1 | `steering` | `steer_player_ships` | Each ship takes the velocity of its held directions; diagonals are as fast as straight lines |
| 2 | `movement` | `apply_velocity` | Every entity moves by `velocity × step` |
| 3 | `sine_motion` | `apply_sine_motion` | Bydos slaves weave up and down on top of their velocity |
| 4 | `play_area` | `clamp_player_ships` | Ships are kept fully on screen |
| 5 | `weapons` | `trigger_weapons` | A ship holding fire with a reloaded weapon fires a missile from its nose |
| 6 | `enemy_fire` | `trigger_auto_weapons` | Each enemy on screen with a reloaded weapon fires a missile to the left |
| 7 | `spawn` | `RandomSpawner::update` | With a seed, Bydos slaves appear at random on the right edge |
| 8 | `waves` | `WaveSchedule::update` | The enemies of the waves file appear when due |
| 9 | `collision` | `CollisionGrid::build`, `find_contacts` | A uniform grid finds the overlapping boxes whose layers can hit each other |
| 10 | `damage` | `apply_damage` | Contacts remove health; missiles are consumed by their first hit; kills are recorded |
| 11 | `score` | `credit_kills` | Each kill is credited to the player who made it (the missile's owner) |
| 12 | `lives` | `handle_player_deaths` | A killed player loses a life and respawns at its slot, or is out |
| 13 | `cleanup` | `remove_casualties` | Dead entities and spent missiles are destroyed |
| 14 | `despawn` | `despawn_outside` | Missiles and enemies that left the screen for good are destroyed |

Movement comes before collisions so that the contacts are those of the positions the
clients will see; damage is applied for both sides of a contact at once, so a ship and a
Bydos that ram each other both take the hit.

## Bydos slaves

A Bydos slave flies left at `tuning::bydos_speed` and weaves around the line it was created
on, with the engine's `SineMotion`: it rises `tuning::bydos_wave_amplitude` (48 px), comes
back down through its line to 48 px below it, and is back on its line every
`tuning::bydos_wave_period` (2 s). The wave depends on the time, not on the number of
ticks, so its path is the same at any tick rate; a missile aimed at its line can pass
above or below it.

## Enemy fire

Bydos slaves carry a `Weapon` with the engine's `AutoTrigger`: they fire on their own, a
missile to the left from their left side, every `tuning::bydos_fire_cooldown` (2 s). The
first shot comes `tuning::bydos_first_shot` (1 s) after the spawn, so the Bydos is on
screen; random Bydos wait up to 2 s more, drawn from the seed, so they do not all fire in
step. An enemy whose center is off screen does not fire. Enemy missiles hit only players: a
hit costs a life, like ramming a Bydos. `enemy_fired()` lists the missiles of the last tick.

## Enemy waves

The waves of a level are data, read when the server starts: `assets/waves/stage1.json` by
default, copied next to `r-type_server`, or another file with `r-type_server --waves
<file>`. Each wave starts some time after the last enemy of the previous one and lists
groups of enemies by type, position, count, interval and offset (a wall, a file, a
diagonal, a V...); `"loop": true` plays them again from the first. The format is the
engine's `WaveSchedule` (see [Add a wave](how-to.md#add-a-wave)). `load_waves()` accepts
only the types of `rtype::enemy_types` (`bydos_slave` for now), so a typo stops the server
at start with the file and the place of the mistake:

```text
r-type_server: assets/waves/stage1.json: waves.1.spawns.0.type: unknown type "bydos" (known: bydos_slave)
```

Wave enemies are the same as the others (`spawn_enemy()` creates them by type): they weave,
fire and despawn. A game plays waves and random spawns together if it has both; one created
without a seed and without waves spawns nothing (tests).

## Enemy spawns

A game created with a seed spawns Bydos slaves on its own, with the engine's
`RandomSpawner`: one every 0.8 to 2 s (`tuning::bydos_spawn_min_interval`,
`tuning::bydos_spawn_max_interval`), just beyond the right edge (`tuning::bydos_spawn_x`),
on a line drawn between `tuning::bydos_lane_top` and `tuning::bydos_lane_bottom` so that
its wave never leaves the screen, and at a random point of its wave. All of it comes from
the seed through the engine's `core::Random`, whose numbers are the same on every platform:
the same seed gives the same enemies at the same ticks, so a game can be replayed from its
logged seed. `spawned()` lists the enemies of the last tick, random ones then those of the
waves.

## Leaving the screen

Missiles and enemies carry the engine's `OffscreenDespawn`: once an entity is entirely
beyond an edge of the screen, by more than `tuning::despawn_margin`, and its velocity does
not bring it back, the `despawn` system destroys it and lists it in `despawned()`. A player
missile is gone just after the right edge, an enemy missile after the left one; a Bydos
created beyond the right edge and flying left is kept until it has crossed the screen. The
margin is larger than the amplitude of the Bydos wave (checked when compiling), so a Bydos
weaving above or below the screen comes back instead of being destroyed.
Players never despawn: they are kept on screen.

## Who can hit whom

Each collider has a `layer` (what it is) and a `mask` (what it can hit), defined in
`rtype/layers.hpp`. Two entities interact when one's mask contains the other's layer.

| Layer | Hits | Hit by |
|---|---|---|
| `player` | enemy (ramming), enemy missile | enemy, enemy missile |
| `player_missile` | enemy | enemy |
| `enemy` | player (ramming), player missile | player, player missile |
| `enemy_missile` | player | player |

There is no friendly fire: players and their missiles ignore each other, and so do enemies.

## Entities

Created by the functions of `rtype/entities.hpp`, all with a `NetworkId` so clients can
name them:

| Entity | Function | Components |
|---|---|---|
| Player ship | `spawn_player(registry, slot)` | Transform, Velocity, PlayerInput, PlayerShip, Sprite, Collider, Health, Damage (ramming), Lives, Score, Weapon |
| Player missile | `spawn_player_missile(registry, shooter)` | Transform, Velocity (right), Sprite, Collider, Damage (consumed on hit), Owner, OffscreenDespawn |
| Bydos slave | `spawn_bydos_slave(registry, line, phase, first_shot)` | Transform, Velocity (left), SineMotion (up and down), Weapon and AutoTrigger (fires on its own), Sprite, Collider, Health, Damage (ramming), ScoreValue, OffscreenDespawn |
| Enemy missile | `spawn_enemy_missile(registry, position)` | Transform, Velocity (left), Sprite, Collider, Damage (consumed on hit), OffscreenDespawn |
| Enemy missile | `fire_enemy_missile(registry, shooter)` | Transform, Velocity (left), Sprite, Collider, Damage (consumed on hit), OffscreenDespawn, Owner (the enemy); placed on its left side |

Players spawn at `x = 100`, their slots spread evenly over the height of the screen.

## Tuning

Every value is a constant of `rtype::tuning` (`rtype/tuning.hpp`):

| Value | Default |
|---|---|
| Tick rate | 60 ticks per second |
| Play area | 1280 × 720 |
| Players | 4 |
| Player speed | 300 px/s |
| Player size, hit box | 33 × 17, 27 × 11 |
| Player health, lives | 1 point, 3 lives |
| Fire rate | one missile every 0.25 s (4 per second) |
| Player missile | 900 px/s, 16 × 4, 1 damage |
| Bydos slave | 120 px/s, 32 × 32, 1 point of health, 100 points |
| Bydos wave | 48 px above and below its line, every 2 s |
| Bydos fire | a missile every 2 s, the first after 1 s (random Bydos: 1 to 3 s) |
| Bydos spawns | every 0.8 to 2 s, at x = 1296, lines from y = 64 to 656 |
| Enemy missile | 300 px/s, 8 × 8, 1 damage |
| Ramming damage | 1 |
| Despawn margin | 64 px beyond each edge |
| Collision grid cell | 64 px |

## Not there yet

- More enemy types (#37 explains how to add one) and the scrolling star field come with
  the next tickets.
