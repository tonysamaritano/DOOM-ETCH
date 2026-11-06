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

The remaining folders will arrive as the port and drawing system are built:

- `platform/darwin/`: native macOS window, Metal, input, sound and main.
- `etch/`: continuous motion paths, silhouettes, hatching and output writers.
- `machine/`: G-code, steppers, physical simulation and requirements.
- `viewer/`: browser rig viewer and promo.
- `tools/`: capture, export and G-code command-line tools.
- `tests/`: automated checks.
- `assets/rig/`: the rig model.
- `test-data/`: ignored local shareware `doom1.wad`; game data is not included.

This layout step preserves engine code unchanged. The build returns in the
next step with CMake; native macOS platform implementations follow it.
