# Trace viewer

Start the read-only local server from the repository:

```sh
python3 viewer/capture_server.py --three /absolute/path/to/three-0.163.0/package
```

The dependency directory must be the complete three.js **0.163.0** package.
It is served through the import map; no bundler, HDRI or remote textures are used.
Open `http://127.0.0.1:8770/viewer/index.html?trace=/path/to/trace.json`.
There is deliberately no default trace. Add `&t=SECONDS` for a paused physical-time view.
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
