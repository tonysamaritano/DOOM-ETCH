# doom-etch

DOOM drawing as one continuous line, like an Etch A Sketch.
Built from id Software's 1997 source release; see LICENSE.TXT.

## Source map

- `engine/core/`: startup, shared types, utilities, memory and I_* interfaces.
- `engine/game/`: gameplay, physics, maps and savegames.
- `engine/render/`: software rendering, video operations and wipes.
- `engine/ui/`: menus, automap, HUD, status bar, intermission and finale.
- `engine/sound/`: engine sound management and sound definitions.
- `engine/wad/`: WAD loading and resource caching.

Platform, test and drawing code use the following folders as the project grows:

- `platform/darwin/`: native macOS window, Metal, input, sound and main.
- `etch/`: continuous motion paths, silhouettes, hatching and output writers.
- `machine/`: G-code, steppers, physical simulation and requirements.
- `viewer/`: browser rig viewer and promo.
- `tools/`: capture, export and G-code command-line tools.
- `tests/`: automated checks.
- `assets/rig/`: the rig model.
- `test-data/`: ignored local shareware `doom1.wad`; game data is not included.

## Build and play on macOS

With the Xcode command-line tools, CMake and a local shareware `doom1.wad` in `test-data/`:

```sh
cmake --preset test -DDOOM_TEST_WAD_DIRECTORY="$PWD/test-data"
cmake --build --preset test
ctest --preset test
DOOMWADDIR=test-data ./build/test/doom-metal -warp 1 1
```

The `sanitize` and `coverage` presets instrument the engine separately.
The unit tests use synthetic fixtures. With the shareware file configured,
`level-smoke` also plays 120 frames of E1M1. Arrow keys move and turn,
Control fires, Space opens doors, and Command-Q quits.
