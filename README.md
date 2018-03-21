# Fixup for the Defold variable_dt Windows bug

Defold [has a bug](https://forum.defold.com/t/variable-dt-causes-game-to-run-twice-as-fast-def-3146/15827)
where timing would be off by a constant factor on Windows platforms.

This native extension attempts a workaround by measuring real time and `dt` time
in tandem for 60 frames, filtering out noisy frames and calculating a factor
to use with `set_time_step`.

## Usage

```lua
dtfixup.init(function (factor)
  msg.post("#collectionproxy", "set_time_step", { factor = factor, mode = 0 })
end)

function update(self, dt)
  dtfixup.update(dt)
end
```
