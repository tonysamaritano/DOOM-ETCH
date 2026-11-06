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

## Run a G-code frame

Export the settled E1M1 view, measure its physical playback, then open it without a WAD:

```sh
./build/test/etch-export test-data/doom1.wad /tmp/e1m1-00
./build/test/etch-gcode-run /tmp/e1m1-00.gcode --machine-readable
./build/test/doom-metal --etch-gcode /tmp/e1m1-00.gcode --etch-time-scale 1000
```

The runner reports completed moves, commanded steps and reversals per motor, and
simulated seconds, including any M0 shake. Calibration values are assumptions,
not measurements. For example, `--settings '$100=40' '$101=40'` changes steps/mm.
Commanded step counts exclude the simulator's separate backlash take-up.

The file window starts at 1× unless `--etch-time-scale` supplies a rate between
1/64× and 1024×. `+`/`=` doubles it and `-`/`_` halves it within those bounds.
F9 pauses, F6 replays from the start at the selected rate, and F7 interrupts
the file and shakes the drawing. An interrupted run does not report full-file
completion; use F6 to start again. Command-Q quits. Playback rate changes presentation only; reported simulated seconds and
motor counts retain their physical values. The rate option requires standalone
`--etch-gcode` mode.

## Play on the rig

Clone the project, put your local shareware `doom1.wad` in `test-data/`, and build:

```sh
git clone https://github.com/tonysamaritano/DOOM-ETCH.git doom-etch
cd doom-etch
cmake --preset test -DDOOM_TEST_WAD_DIRECTORY="$PWD/test-data"
cmake --build --preset test
DOOMWADDIR=test-data ./build/test/doom-metal --play-on-etch --fullscreen -warp 1 1
```

The embedded local rig uses the full captured drawing, including the HUD, weapon,
sprites and hatching. It starts in instant visual mode. Ctrl-Command-F toggles
fullscreen; arrow keys move and turn, Control fires, Space uses doors, and Command-Q
quits. Sound effects use the Mac's audio output; music is not implemented.

F8 switches instant/timed drawing, +/- changes its presentation rate within
1/64×–1024×, F9 pauses the drawing and F7 erases it into a faint ghost.
F2 changes hatch pattern, F3 sprite hatching, F4 wall hatching, F5 sprites,
H the HUD, G the gun and J gun hatching. Layer changes clear the visual ghost.
