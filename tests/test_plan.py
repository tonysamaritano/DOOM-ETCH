"""Small independent geometry and command-line checks for the Python planner."""
import collections
import importlib.util
import math
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/etch/plan.py'
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location('wall_plan', SCRIPT)
plan = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(plan)


class PlannerTests(unittest.TestCase):
    def capture(self, directory, rows, width=3, height=3):
        capture = Path(directory) / 'walls.csv'
        capture.write_text(f'# doom-wall-bands-v1,{width},{height}\n' + rows)
        return capture
