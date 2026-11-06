// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdlib.h>
#include <string.h>
#include "r_etch.h"

enum { CAPTURE_WIDTH = 320, CAPTURE_HEIGHT = 200 };
static int cells[CAPTURE_WIDTH * CAPTURE_HEIGHT];
static int active;
static int width, height;
int R_EtchBegin(int w, int h)
{
    if (active || w <= 0 || w > CAPTURE_WIDTH || h <= 0 || h > CAPTURE_HEIGHT)
        return 0;
    width = w; height = h;
    memset(cells, 0, (size_t)width * height * sizeof(*cells));
    active = 1;
    return 1;
}

void R_EtchColumn(int x, int top, int bottom, int surface)
{
    if (!active || x < 0 || x >= width || surface <= 0) return;
    if (top < 0) top = 0;
    if (bottom >= height) bottom = height - 1;
    // BSP rendering reaches the nearest visible wall first.
    for (int y = top; y <= bottom; y++)
        if (!cells[y * width + x]) cells[y * width + x] = surface;
}
