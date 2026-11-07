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
