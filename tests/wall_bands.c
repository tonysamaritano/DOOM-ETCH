#include "r_etch.h"
#include "motion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int samePoint(MotionPoint a, MotionPoint b)
{
    return fabs(a.x - b.x) < 1e-9 && fabs(a.y - b.y) < 1e-9;
}






int main(void)
{
    outerCell();
    adjacentSurfaces();
    nearWallWins();
    clippedBands();
    captureLifecycle();
    puts("Wall bands: boundaries, seams, clipping and capture lifecycle passed");
    return 0;
}
