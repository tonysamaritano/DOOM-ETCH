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

With the Xcode command-line tools, CMake, Python 3 for CLI tests, and a local
shareware `doom1.wad` in `test-data/`:

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

Add `--etch-via-gcode` to drive one persistent simulated device through the actual
G-code writer, parser and step compiler. Ink, knobs, motors and belts follow the
same physical state, including backlash. This option advances at physical **1×**;
the game stays responsive while the current drawing and its shake finish.
F9 pauses physical drawing without catch-up. F7 or a layer change finishes the
current segment, cancels the rest and requests the existing physical shake.
F8 and +/- do not accelerate this mode; its badge and title identify physical 1×.
Physical mode rejects `--etch-gpu-rate` and `--etch-gpu-instant` at startup.
`--etch-time-scale` remains restricted to standalone `--etch-gcode FILE` playback.
Calibration is assumed: 20 steps/mm, 60 mm/s and 250 mm/s² per axis,
0.15 mm backlash, 20 mm per knob turn, and a 2.5-second shake retaining 3% of marks.
The physical clock reports simulated duration under those assumptions.

Each build refreshes an `etch-play/` resource directory beside `doom-metal`, including
the model, both note textures, required three.js modules and their MIT license.
Keep that directory with the executable when moving it. The local custom URL scheme
needs no web server, bundler, internet connection or working-directory-specific assets.
The visible model opening is 178.4 × 127.6 mm; the drawing covers it edge to edge.
Physical travel and all controller calculations stay at 170.15 × 127.6 mm.

## Measure the required drawing speed

Request drawing demands at a target frame rate alongside the physical result:

```sh
./build/test/etch-export test-data/doom1.wad /tmp/e1m1 --target-fps 30
./build/test/etch-gcode-run /tmp/e1m1.gcode --target-fps 30 --machine-readable --trace /tmp/e1m1.json
./build/test/etch-gcode-run /tmp/e1m1.gcode --target-fps 35
```

Both tools print these demands only with `--target-fps`. Default stdout retains its
physical report. Trace JSON always includes a `requirements` array at the requested
fps, or 30 fps when omitted. Frame IDs start at zero and preserve empty M0 frames.
Each frame gets its own drawing-only 1/fps budget, retaining its starting cursor.
Target fps affects reporting only; commands, step counts and physical clocks retain
their values. The existing PATH line remains the raw, unquantized ideal-feed estimate.

For commanded segment distances in metres, length L and sum S of square roots,
no-stop speed is L×fps and stop-at-every-vertex acceleration is (2×S×fps)².
Peak speed is √(acceleration×longest segment). Mach uses an assumed 343 m/s;
g uses standard gravity 9.80665 m/s². Knob RPM is a path-speed-equivalent demand
using the assumed mm per knob turn; separate-axis and motor-pulley speeds differ.
Backlash, shake and jerk are excluded from this drawing-demand calculation.
A 100 mm straight line at 30 fps requires 3 m/s without stops, or 360 m/s²
and 6 m/s peak with stops, equivalent to 9,000 knob rpm at 20 mm per turn.
Nonfinite rates and unrepresentable positive demands fail with a diagnostic.
