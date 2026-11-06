# Trace viewer

Start the read-only local server from the repository:

```sh
python3 viewer/capture_server.py
```

The required browser modules from three.js **0.163.0** and its MIT license are
vendored in `viewer/vendor/three/`. This is a browser module subset, not an npm
development installation. The import map uses those local files by default;
`--three /absolute/package/path` can override them with the same version.
No bundler, HDRI or remote textures are used.
Build the test preset and put shareware `doom1.wad` in ignored `test-data/`, then run:

```sh
tools/rig_frame.sh
```

The script exports E1M1, runs the controller and writes `build/viewer/e1m1.json`.
Open its printed URL and press Play. The viewer defaults to this complete frame.
Use `?trace=/path/to/trace.json` to select another drawing, or `?t=SECONDS` to hold
an exact physical moment in the default drawing (`&t=SECONDS` after another query).
Play advances at real **1×**, with pause, restart and seek. There is no rate control.
Camera lens, aperture and focus change the rendered optics; Reset camera restores framing.

Trace coordinates and calibration remain physical millimetres and seconds.
The model's visible `DrawArea` is 178.4 × 127.6 mm. Logical 170.15 × 127.6 mm travel
maps into that opening with X factor 1.0484866294; this never changes physical data.
Knobs and 3:1 motor pivots use continuous motor travel, including backlash.
Belts transport their vertices around their actual pulley pitch loops.
The stylus marker uses the configured per-axis step grid and forced endpoints.

Swept capsules approximate visible ink, rather than reproducing the simulator's discrete
DDA mark grid. Their 0.5 mm display width follows the prototype. Historical strokes use
0.08 visual opacity per shake, distinct from the physical retained-mark fraction.
The hidden `#viewer-evidence` DOM record exposes actual asset/trace URLs, errors, physical
positions, drive angles, belt phase, mapping, canvas dimensions and optics for verification.

At an exact half-step crossing, subtracting a global floating-point time can choose
an adjacent discrete step. The viewer uses the configured nearest-step arithmetic
without an extra tolerance; continuous motor travel and the recorded clock stay unchanged.
