# Rig model

`etch-rig.glb` contains the finished two-motor rig and its embedded textures.
The drives turn press-fit knob pulleys from stepper motors on a T-slot easel.

| Part | Model value |
| --- | --- |
| Knob pulley | 54 teeth |
| Motor pulley | 18 teeth |
| Drive ratio | 3 motor revolutions per knob revolution |
| Timing belts | T2.5 pitch, 220 mm pitch length, 6 mm wide |
| Stepper motor | SOYO SY42STH38-1684A |

`Screen` is the screen mesh. Its child `DrawArea` marks the top-left origin:
local +X runs right, +Y runs up the screen, and +Z points outward. The model
retains Z-up coordinates (`source_up: "Z"`); viewers must orient it explicitly.
`KnobX`/`KnobY` and `MotorX`/`MotorY` rotate around their local Z axes. Motor
extras record the axis, 54/18 tooth counts, 2.5 mm pitch and 3:1 ratio.

The copied `DrawArea` metadata describes a **178.4 × 127.6 mm visible opening**.
It is visual geometry, not calibrated stylus travel. Its archival note assumes
reachable travel equals that opening; this product supersedes that assumption:
**physical drawing travel is 170.15 × 127.6 mm**. Use the trace's physical
extents for controller coordinates and reported measurements, independently
of how a viewer maps them onto the visible model.

Pulley geometry and belt length describe the modeled parts. Shaft-center
spacing, tension, knob torque and travel per knob revolution remain hardware
assumptions to verify on a real build; the model does not measure them.
