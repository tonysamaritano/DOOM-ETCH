// SPDX-License-Identifier: GPL-2.0-or-later
#include "r_local.h"

// Emit the transitions that open and close horizontal spans for one plane.
void R_EmitPlaneSpans(visplane_t *plane)
{
    int previousTop = 0xff, previousBottom = 0;
    for (int x = plane->minx; x <= plane->maxx; x++) {
        int top = plane->top[x], bottom = plane->bottom[x];
        R_MakeSpans(x, previousTop, previousBottom, top, bottom);
        previousTop = top;
        previousBottom = bottom;
    }
    R_MakeSpans(plane->maxx + 1, previousTop, previousBottom, 0xff, 0);
}
