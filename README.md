# rul-godot

Wave function collapse for Godot, as a GDExtension.

Godot 4.7+. Linux x86_64, Windows x86_64, macOS universal.

## Install

Grab the zip from [releases](../../releases) and extract it into your project root, so you
end up with `addons/rul/`.

## Usage

States are plain ints, so a GDScript enum is all you need. Say what may sit next to what,
then generate:

```gdscript
enum Tile { WATER, SAND, GRASS }

var wfc := WaveFunctionCollapse.new()

wfc.allow_all(Tile.WATER, Tile.WATER)
wfc.allow_all(Tile.WATER, Tile.SAND)
wfc.allow_all(Tile.SAND, Tile.SAND)
wfc.allow_all(Tile.SAND, Tile.GRASS)
wfc.allow_all(Tile.GRASS, Tile.GRASS)
# water and grass are never allowed to touch, so you always get a beach

wfc.set_weight(Tile.WATER, 1.5)  # water shows up half again as often as the rest

var grid := wfc.generate(Vector2i(32, 32), 1234)
for y in 32:
    for x in 32:
        $TileMapLayer.set_cell(Vector2i(x, y), 0, atlas_coords[grid[y * 32 + x]])
```

`generate()` returns a `PackedInt32Array` in row-major order, so cell `(x, y)` is at
`y * size.x + x`. The same seed and rules always give the same grid.

It returns an **empty array** when the rules leave some cell with no legal state. That is
normal for wave function collapse — retry with another seed:

```gdscript
var seed := 0
var grid := wfc.generate(size, seed)
while grid.is_empty() and seed < 20:
    seed += 1
    grid = wfc.generate(size, seed)
```

`allow_all(a, b)` permits the pair on all four sides. Use
`allow(a, WaveFunctionCollapse.NORTH, b)` when a rule is directional — a cliff face only
works one way up. Either way the mirrored rule is recorded too, so `NORTH` for `a` implies
`SOUTH` for `b`.

See `demo/main.gd` for a runnable version.

## Build

```sh
git clone --recurse-submodules <repo-url> && cd rul-godot
cmake --preset godot-debug && cmake --build --preset godot-debug
```

Output lands in `demo/addons/rul/bin/`. Use the `godot-release` preset for release builds,
and `tests` + `ctest --preset tests` for the core unit tests (no Godot needed).

`build_profile.json` lists the engine classes godot-cpp generates bindings for, which is
what keeps a build at ~15s instead of ~10min. Anything you `#include` has to be in there,
and it's read at configure time — rerun `cmake --preset ...` after editing it, a plain
rebuild won't notice.
