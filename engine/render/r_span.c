// SPDX-License-Identifier: GPL-2.0-or-later
#include "r_local.h"

// Emit the transitions that open and close horizontal spans for one plane.
void R_EmitPlaneSpans(visplane_t *plane)
{
    plane->top[plane->maxx + 1] = 0xff;
    plane->top[plane->minx - 1] = 0xff;
    int stop = plane->maxx + 1;
    for (int x = plane->minx; x <= stop; x++) {
        R_MakeSpans(x, plane->top[x - 1], plane->bottom[x - 1],
            plane->top[x], plane->bottom[x]);
    }
}
