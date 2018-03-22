# Fixup for the Defold variable_dt Windows bug

Defold [has a bug](https://forum.defold.com/t/variable-dt-causes-game-to-run-twice-as-fast-def-3146/15827)
where timing would be off by a constant factor on Windows platforms.

This native extension attempts a workaround by measuring real time and `dt` time
in tandem for 60 frames, filtering out noisy frames and calculating a factor
to use with `set_time_step`.

## Usage

Add https://github.com/dapetcu21/defold-dtfixup/archive/master.zip to your dependencies, then:

```lua
dtfixup.init(function (factor)
  msg.post("#collectionproxy", "set_time_step", { factor = factor, mode = 0 })
end)

function update(self, dt)
  dtfixup.update(dt)
end
```

The callback passed to `dtfixup.init()` will be called as soon as a measurement is available (after 10 frames) and will be called again with refined values the next 50 frames.

This extension reads `variable_dt` and doesn't attempt to correct the time step if it's disabled or if the platform is not Windows.
