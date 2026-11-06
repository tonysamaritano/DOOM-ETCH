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

    def test_simplification_keeps_ends_and_large_corners(self):
        self.assertEqual(plan.simplify([(0, 0), (1, 0), (2, 0)]), [(0, 0), (2, 0)])
        corner = [(0, 0), (0, 3), (3, 3), (3, 0), (0, 0)]
        self.assertEqual(plan.simplify(corner), corner)
        self.assertEqual(plan.simplify([(0, 0)]), [(0, 0)])

    def test_physical_canvas_and_empty_capture(self):
        points, kinds = plan.make_plan([[1, 0, 2]])
        self.assertEqual(points[0], (0, 0))
        self.assertEqual(len(points), len(kinds) + 1)
        self.assertTrue(all(0 <= x <= 170.15 and 0 <= y <= 127.6 for x, y in points))
        self.assertAlmostEqual(max(x for x, _ in points), 170.15)
        self.assertAlmostEqual(max(y for _, y in points), 127.6)
        self.assertTrue(math.isfinite(sum(math.dist(a, b) for a, b in zip(points, points[1:]))))
        with self.assertRaises(ValueError):
            plan.make_plan([[0, 0]])

    def test_cli_writes_one_path_with_colored_connector_overlays(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = self.capture(directory, '0,0,0,1\n2,0,0,2\n', height=1)
            prefix = Path(directory) / 'drawing'
            result = subprocess.run([sys.executable, str(SCRIPT), str(capture),
                                     '--output', str(prefix)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(' mm;', result.stdout)
            self.assertEqual({file.name for file in Path(directory).iterdir()},
                             {'walls.csv', 'drawing.svg'})
            root = ET.parse(prefix.with_suffix('.svg')).getroot()
            namespace = {'svg': 'http://www.w3.org/2000/svg'}
            paths = root.findall('svg:path', namespace)
            self.assertEqual(len(paths), 1)
            data = paths[0].attrib['d']
            self.assertEqual(re.findall('[A-Za-z]', data).count('M'), 1)
            self.assertEqual(set(re.findall('[A-Za-z]', data)), {'M', 'L'})
            connectors = root.findall('svg:line', namespace)
            self.assertEqual(len(connectors), 1)
            self.assertNotEqual(connectors[0].attrib['stroke'], paths[0].attrib['stroke'])
            points, kinds = plan.make_plan(plan.load_bands(capture))
            self.assertEqual(data.count('L'), len(kinds))
            a, b = next((a, b) for a, b, kind in zip(points, points[1:], kinds)
                        if kind == 'connector')
            line = connectors[0].attrib
            self.assertAlmostEqual(float(line['x1']), a[0], places=5)
            self.assertAlmostEqual(float(line['x2']), b[0], places=5)
            self.assertAlmostEqual(float(line['y1']), a[1], places=5)
            self.assertAlmostEqual(float(line['y2']), b[1], places=5)


if __name__ == '__main__':
    unittest.main()
