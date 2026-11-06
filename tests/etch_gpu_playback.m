/* Whitebox timing and actual offscreen Metal checks; no window or drawable. */
#import "../platform/darwin/etch_gpu.m"
#include <assert.h>
#include <stdio.h>

static double testClock;
static EtchGPUPath *fixturePath(float firstY)
{
    MotionTimedSegment records[] = {
        {{.1f, firstY, .4f, firstY}, {0, 1, 0, 0}},
        {{.6f, firstY + .5f, .9f, firstY + .5f}, {1, 2, 0, 0}}
    };
    EtchGPUPath *path = [EtchGPUPath new];
    path.buffer = [device newBufferWithBytes:records length:sizeof(records)
                                     options:MTLResourceStorageModeShared];
    assert(path.buffer); path.count = 2; path.duration = 2;
    return path;
}
static void startFixture(void)
{
    EtchGPUInit(MTLCreateSystemDefaultDevice(), 1200);
    const char *error = EtchGPUError();
    if (error && *error) fprintf(stderr, "%s\n", error);
    assert(!error || !*error);
    current = fixturePath(.25f); needsPath = NO;
    testClock = 10; EtchGPUTick(testClock, YES);
}
static void advance(double seconds)
{
    while (seconds > 1e-10) {
        double step = fmin(.25, seconds);
        testClock += step; EtchGPUTick(testClock, YES); seconds -= step;
    }
}
