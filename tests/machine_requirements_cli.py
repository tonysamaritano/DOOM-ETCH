# SPDX-License-Identifier: GPL-2.0-or-later
"""Independent output-level requirements checks; not part of drawing runtime."""
import argparse
import json
import math
from pathlib import Path
import subprocess


def invoke(executable, args, success=True):
    result = subprocess.run([str(executable), *map(str, args)],
                            capture_output=True, text=True, timeout=120)
    assert (result.returncode == 0) == success, (args, result.stdout, result.stderr)
    return result


def summary(runner, path, *args):
    result = invoke(runner, [path, '--machine-readable', *args])
    record = json.loads(result.stdout)  # Reject extra objects / trailing logs.
    assert record['type'] == 'machine-summary'
    return result.stdout, record


def close(actual, expected):
    assert math.isfinite(actual)
    assert math.isclose(actual, expected, rel_tol=2e-11, abs_tol=2e-12), (actual, expected)


def write_fixture(path):
    path.write_text('; ETCH_SIZE 170.15 127.6\n; ETCH_START 10 117.6\n'
                    'G21\nG90\nG94\nG61\nF3600\nM0\n'
                    'G1 X110 Y117.6\nG1 X110 Y117.6\nM0\nM0\n'
                    'G0 X10 Y117.6\nM0\nM2\n')


def check_frames(records, fps):
    assert [r['frame'] for r in records] == list(range(5))
    for i, r in enumerate(records):
        assert r['type'] == 'machine-requirements'
        assert r['target_fps'] == fps
        assert r['geometry'] == 'commanded-step-grid'
        assert r['budget'] == 'drawing-only-one-frame'
        assert r['segments'] == {1: 2, 3: 1}.get(i, 0)
        active = i in (1, 3)
        close(r['path_m'], .1 if active else 0)
        close(r['no_stop_m_s'], (.1 * fps) if active else 0)
        close(r['accel_m_s2'], (360 if fps == 30 else 490) if active else 0)
        close(r['peak_m_s'], (6 if fps == 30 else 7) if active else 0)
        close(r['knob_rpm'], (9000 if fps == 30 else 10500) if active else 0)
        assert r['assumptions'] == {'sound_speed_m_s': 343,
            'gravity_m_s2': 9.80665, 'knob_mm_turn': 20,
            'knob_rpm': 'path-speed-equivalent'}
        assert set(r['exclusions']) == {'backlash', 'shake', 'jerk'}


def trace_compatibility(runner, fixture, work, original):
    baseline = None
    for fps in (None, 30, 35):
        target = work / f'trace-{fps}.json'
        options = ['--trace', target]
        if fps is not None:
            options += ['--target-fps', fps]
        raw, record = summary(runner, fixture, *options)
        trace = json.loads(target.read_text())
        check_frames(trace.pop('requirements'), fps or 30)
        if fps is None:
            assert raw == original
            baseline = trace
        else:
            check_frames(record.pop('requirements'), fps)
            assert trace == baseline  # Every old field, not just duration.


def report_compatibility(runner, fixture, work):
    original, physical = summary(runner, fixture)
    assert 'requirements' not in physical
    plain = invoke(runner, [fixture]).stdout
    for fps in (30, 35):
        _, record = summary(runner, fixture, '--target-fps', fps)
        check_frames(record.pop('requirements'), fps)
        assert record == physical  # Includes calibration / steps / exact clock.
        explicit = invoke(runner, [fixture, '--target-fps', fps]).stdout
        assert explicit.startswith(plain)
        assert 'requirements' in explicit and 'RPM' in explicit
    trace_compatibility(runner, fixture, work, original)


def invalid_flags(executable, initial, work):
    sentinel = work / 'must-not-publish.json'
    sentinel.write_text('previous trace\n')
    bad = ('0', '-1', 'nan', 'inf', '30x', ' 30', '30 ', '0x1p5',
           '1e9999', '1e-9999', '')
    variants = [['--target-fps', value] for value in bad]
    variants += [['--target-fps'], ['--target-fps', '30', '--target-fps', '35']]
    for flags in variants:
        result = invoke(executable, [*initial, *flags], success=False)
        assert '--target-fps' in result.stderr, (flags, result.stderr)
        assert not result.stdout, result.stdout
        assert sentinel.read_text() == 'previous trace\n'


def empty_files(runner, work):
    for name, text in (('empty', ''), ('end', 'M2\n'), ('comments', '; hi\n')):
        path = work / f'{name}.gcode'
        path.write_text(text)
        _, report = summary(runner, path, '--target-fps', 30)
        assert len(report['requirements']) == 1
        r = report['requirements'][0]
        assert r['frame'] == 0 and not r['segments'] and not r['path_m']
