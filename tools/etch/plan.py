#!/usr/bin/env python3
"""Turn clipped Doom wall bands into one continuous pen-down SVG drawing."""
import argparse
import math
from pathlib import Path

TRAVEL_WIDTH = 170.15
TRAVEL_HEIGHT = 127.6
SIMPLIFY_PIXELS = 0.7


def load_bands(filename):
    with Path(filename).open() as stream:
        header = stream.readline().strip().split(',')
        if len(header) != 3 or header[0] != '# doom-wall-bands-v1':
            raise ValueError('Expected a doom-wall-bands-v1 capture')
        width, height = map(int, header[1:])
        if not (1 <= width <= 2048 and 1 <= height <= 2048):
            raise ValueError('Capture dimensions must be in 1..2048')
        cells = [[0 for _ in range(width)] for _ in range(height)]
        for row, text in enumerate(stream, 2):
            values = text.strip().split(',')
            if len(values) != 4:
                raise ValueError(f'Row {row}: expected x,top,bottom,surface')
            x, top, bottom, surface = map(int, values)
            if not (0 <= x < width and 0 <= top <= bottom < height and surface > 0):
                raise ValueError(f'Row {row}: wall band is outside the capture')
            for y in range(top, bottom + 1):
                if cells[y][x] == 0:  # Renderer order is nearest first.
                    cells[y][x] = surface
    return cells


def structural_edges(cells):
    height, width = len(cells), len(cells[0])
    edges = set()
    for y in range(height):
        for x in range(width):
            surface = cells[y][x]
            above = cells[y - 1][x] if y else 0
            left = cells[y][x - 1] if x else 0
            if surface != above:
                edges.add(((x, y), (x + 1, y)))
            if surface != left:
                edges.add(((x, y), (x, y + 1)))
            if y == height - 1 and surface:
                edges.add(((x, y + 1), (x + 1, y + 1)))
            if x == width - 1 and surface:
                edges.add(((x + 1, y), (x + 1, y + 1)))
    return edges


def route_edges(edges):
    neighbors = {}
    for a, b in edges:
        neighbors.setdefault(a, set()).add(b)
        neighbors.setdefault(b, set()).add(a)
    remaining = set(neighbors)
    points, kinds = [(0, 0)], []
    while remaining:
        cursor = points[-1]
        root = min(remaining, key=lambda p: ((p[0] - cursor[0]) ** 2 + (p[1] - cursor[1]) ** 2, p))
        if root != cursor:
            points.append(root)
            kinds.append('connector')
        stack = [root]
        while stack:
            vertex = stack[-1]
            if neighbors[vertex]:
                other = min(neighbors[vertex])
                neighbors[vertex].remove(other)
                neighbors[other].remove(vertex)
                for endpoint in (vertex, other):
                    if not neighbors[endpoint]:
                        remaining.discard(endpoint)
                points.append(other)
                kinds.append('outline')
                stack.append(other)
            else:
                remaining.discard(vertex)
                stack.pop()
                if stack:
                    points.append(stack[-1])
                    kinds.append('outline')
    return points, kinds


def simplify(points, tolerance=SIMPLIFY_PIXELS):
    """Retain corners farther than tolerance from a candidate straight segment."""
    keep = {0, len(points) - 1}
    pending = [(0, len(points) - 1)]
    while pending:
        first, last = pending.pop()
        a, b = points[first], points[last]
        dx, dy = b[0] - a[0], b[1] - a[1]
        length_squared = dx * dx + dy * dy
        farthest, distance_squared = None, tolerance * tolerance
        for index in range(first + 1, last):
            px, py = points[index][0] - a[0], points[index][1] - a[1]
            projection = (px * dx + py * dy) / length_squared if length_squared else 0
            projection = min(1, max(0, projection))
            distance = (px - projection * dx) ** 2 + (py - projection * dy) ** 2
            if distance > distance_squared:
                farthest, distance_squared = index, distance
        if farthest is not None:
            keep.add(farthest)
            pending.extend(((first, farthest), (farthest, last)))
    return [points[index] for index in sorted(keep)]


def make_plan(cells):
    edges = structural_edges(cells)
    if not edges:
        raise ValueError('Capture contains no wall outlines')
    points, kinds = route_edges(edges)
    result, result_kinds = [points[0]], []
    start = 0
    while start < len(kinds):
        end = start + 1
        while end < len(kinds) and kinds[end] == kinds[start]:
            end += 1
        run = points[start:end + 1]
        if kinds[start] == 'outline':
            run = simplify(run)
        result.extend(run[1:])
        result_kinds.extend([kinds[start]] * (len(run) - 1))
        start = end
    width, height = len(cells[0]), len(cells)
    physical = [(x * TRAVEL_WIDTH / width, y * TRAVEL_HEIGHT / height)
                for x, y in result]
    return physical, result_kinds


def write_svg(points, kinds, prefix):
    destination = Path(str(prefix) + '.svg')
    destination.parent.mkdir(parents=True, exist_ok=True)
    coordinates = ' L '.join(f'{x:.6f},{y:.6f}' for x, y in points)
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{TRAVEL_WIDTH}mm" '
             f'height="{TRAVEL_HEIGHT}mm" viewBox="0 0 {TRAVEL_WIDTH} {TRAVEL_HEIGHT}">',
             '<title>Doom continuous wall drawing</title>',
             f'<path d="M {coordinates}" fill="none" stroke="#30332f" '
             'stroke-width="0.15" stroke-linecap="round" stroke-linejoin="round"/>']
    for a, b, kind in zip(points, points[1:], kinds):
        if kind == 'connector':
            lines.append(f'<line x1="{a[0]:.6f}" y1="{a[1]:.6f}" '
                         f'x2="{b[0]:.6f}" y2="{b[1]:.6f}" '
                         'stroke="#df792d" stroke-width="0.2"/>')
    destination.write_text('\n'.join(lines + ['</svg>', '']))
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        points, kinds = make_plan(load_bands(args.capture))
        output = write_svg(points, kinds, args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')
    length = sum(math.dist(a, b) for a, b in zip(points, points[1:]))
    print(f'{len(kinds)} moves; {length:.6f} mm; {kinds.count("connector")} connectors')
    print(output)


if __name__ == '__main__':
    main()
