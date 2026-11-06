// SPDX-License-Identifier: GPL-2.0-or-later
#include <assert.h>
#include <string.h>
#include "r_local.h"

static int calls, transitions[SCREENWIDTH + 1][5];
void R_MakeSpans(int x, int top1, int bottom1, int top2, int bottom2)
{
    assert(calls <= SCREENWIDTH);
    int *args = transitions[calls++];
    args[0] = x; args[1] = top1; args[2] = bottom1;
    args[3] = top2; args[4] = bottom2;
}
static void Check(int first, int last)
{
    visplane_t plane, before;
    memset(&plane, 0x5a, sizeof(plane));
    plane.minx = first; plane.maxx = last;
    for (int x = first; x <= last; x++) {
        plane.top[x] = 2 + x % 3;
        plane.bottom[x] = 6 + x % 3;
    }
    memcpy(&before, &plane, sizeof(before)); calls = 0;
    R_EmitPlaneSpans(&plane);
    assert(calls == last - first + 2);
    assert(!memcmp(&plane, &before, sizeof(plane)));
    for (int i = 0; i < calls; i++) {
        int x = first + i, *args = transitions[i];
        assert(args[0] == x);
        assert(args[1] == (x == first ? 255 : before.top[x - 1]));
        assert(args[2] == (x == first ? 0 : before.bottom[x - 1]));
        assert(args[3] == (x == last + 1 ? 255 : before.top[x]));
        assert(args[4] == (x == last + 1 ? 0 : before.bottom[x]));
    }
}
int main(void) { Check(0, SCREENWIDTH - 1); Check(3, SCREENWIDTH - 4); return 0; }
