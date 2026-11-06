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












if __name__ == '__main__':
    main()
