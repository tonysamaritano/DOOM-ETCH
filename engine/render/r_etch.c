// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdio.h>
#include "r_etch.h"

static FILE *output;
static int width, height;
int R_EtchBegin(const char *path, int w, int h)
{
    if (output || !path || w <= 0 || h <= 0) return 0;
    output = fopen(path, "w");
    if (!output) return 0;
    width = w; height = h;
    fprintf(output, "# doom-wall-bands-v1,%d,%d\n", width, height);
    return 1;
}
void R_EtchColumn(int x, int top, int bottom, int surface)
{
    if (!output || x < 0 || x >= width) return;
    if (top < 0) top = 0;
    if (bottom >= height) bottom = height - 1;
    if (top <= bottom)
        fprintf(output, "%d,%d,%d,%d\n", x, top, bottom, surface);
}
int R_EtchEnd(void)
{
    if (!output) return 0;
    int failed = ferror(output);
    if (fclose(output)) failed = 1;
    output = NULL;
    return !failed;
}
