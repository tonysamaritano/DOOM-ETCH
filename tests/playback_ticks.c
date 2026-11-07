// SPDX-License-Identifier: GPL-2.0-or-later
/* A window feeds playback whatever time passed since its last frame. Uneven
 * ticks must still finish the file, with the same physical time as one tick. */
#include "run.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double run(const char *file, unsigned seed)
{
    MachineParseError error = {0};
    MachinePlayback *play = MachineLoad(file, NULL, 0, &error);
    assert(play);
    unsigned state = seed;
    while (!MachinePlaybackDone(play)) {
        double tick = 1e6;
        if (seed) {  /* Mostly 60 Hz frames at 1000x, with short and long stalls. */
            state = state * 1664525u + 1013904223u;
            double r = (state >> 8) / 16777216.0, kind = (state >> 4) % 10;
            tick = 1000 * (kind == 0 ? .001 + .002 * r : kind == 1 ? .05 + .3 * r : 1.0 / 60 + (r - .5) * .004);
        }
        if (!MachinePlaybackTick(play, tick)) {
            fprintf(stderr, "seed %u: %s at %.9f s\n", seed,
                MachinePlaybackError(play)->message, MachinePlaybackSeconds(play));
            abort();
        }
    }
    double seconds = MachinePlaybackSeconds(play);
    MachinePlaybackDestroy(play);
    return seconds;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *quiet = freopen("/dev/null", "w", stdout); assert(quiet);
    double whole = run(argv[1], 0);
    for (unsigned seed = 1; seed <= 150; ++seed)
        assert(fabs(run(argv[1], seed) - whole) <= 1e-9 * whole);
    fprintf(stderr, "150 uneven playbacks finished at %.9f s\n", whole);
    return 0;
}
