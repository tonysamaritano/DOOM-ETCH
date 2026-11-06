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

    def test_nearest_wall_wins_overlapping_bands(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = self.capture(directory, '1,1,1,4\n1,0,2,9\n')
            self.assertEqual(plan.load_bands(capture), [[0, 9, 0], [0, 4, 0], [0, 9, 0]])

    def test_rejects_invalid_capture(self):
        bad = ['# unknown,3,3\n', '# doom-wall-bands-v1,0,3\n',
               '# doom-wall-bands-v1,3,3\n3,0,1,2\n',
               '# doom-wall-bands-v1,3,3\n1,2,1,2\n',
               '# doom-wall-bands-v1,3,3\n1,0,1,0\n',
               '# doom-wall-bands-v1,3,3\n1,0,1\n']
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / 'bad.csv'
            for text in bad:
                with self.subTest(text=text):
                    filename.write_text(text)
                    with self.assertRaises(ValueError):
                        plan.load_bands(filename)

    def test_boundaries_include_outer_edges_and_surface_seams(self):
        edges = plan.structural_edges([[1, 2]])
        expected = {((0, 0), (1, 0)), ((1, 0), (2, 0)),
                    ((0, 1), (1, 1)), ((1, 1), (2, 1)),
                    ((0, 0), (0, 1)), ((1, 0), (1, 1)), ((2, 0), (2, 1))}
        self.assertEqual(edges, expected)
        self.assertEqual(plan.structural_edges([[0]]), set())

    def test_raw_route_traverses_each_outline_twice(self):
        edges = plan.structural_edges([[1, 0, 2]])
        points, kinds = plan.route_edges(edges)
        self.assertEqual(len(points), len(kinds) + 1)
        visits = collections.Counter()
        connectors = 0
        for a, b, kind in zip(points, points[1:], kinds):
            if kind == 'outline':
                visits[tuple(sorted((a, b)))] += 1
            else:
                connectors += 1
        self.assertEqual(set(visits), {tuple(sorted(edge)) for edge in edges})
        self.assertTrue(all(count == 2 for count in visits.values()))
        self.assertEqual(connectors, 1)
        self.assertEqual(plan.route_edges(set(reversed(sorted(edges)))), (points, kinds))





if __name__ == '__main__':
    unittest.main()
