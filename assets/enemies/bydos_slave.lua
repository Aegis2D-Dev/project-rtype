-- Bydos slave, the first enemy of R-Type: it flies left while weaving up and down around its
-- line, and fires every 2 s. The server runs this file; edit it to change the Bydos, no
-- recompilation needed (docs/add-an-enemy.md).

local speed = 120    -- to the left, in logical pixels per second
local amplitude = 48 -- largest distance above or below its line; at most 64, or a Bydos
                     -- weaving off screen is despawned before it comes back
local period = 2     -- seconds to weave up, down and back to its line

-- Where its weave puts it above its line, `time` seconds into it: it first rises (towards
-- y = 0).
local function weave(time)
    return -amplitude * math.sin(2 * math.pi * time / period)
end

enemy {
    width = 32, height = 32, -- tuning::bydos_size, which the clients draw
    health = 1,
    score = 100,
    fire_every = 2,
    -- It enters about 0.3 s after spawning beyond the right edge; random Bydos wait up to
    -- one more fire_every, so they do not fire in step.
    first_shot = 1,

    -- `self.random` picks how far into its weave it starts, so that Bydos spawned together
    -- at random do not move in step; the waves spawn them on their line (random = 0).
    spawn = function(self)
        self.line = self.y
        self.time = self.random * period
        ecs.get(self.entity, "Velocity").x = -speed
        ecs.get(self.entity, "Transform").y = self.line + weave(self.time)
    end,

    update = function(self, dt)
        self.time = self.time + dt
        ecs.get(self.entity, "Transform").y = self.line + weave(self.time)
    end,
}
