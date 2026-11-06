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

static void manualEraseFreezesRevealedPrefix(void)
{
    startFixture(); ghost = fixturePath(.15f); ghostTime = 2;
    advance(.5); assert(fabs(playhead - .5) < 1e-9);
    EtchGPUErase(); EtchGPUTick(testClock, YES);
    assert(erasing && fabs(eraseTime) < 1e-9);
    advance(.325);
    assert(fabs(playhead - .5) < 1e-9 && !EtchGPUNeedsPath());
    EtchGPUFrame *fading = EtchGPUFrameState();
    assert(fabs(fading.currentTime - .5) < 1e-9);
    assert(fabs(fading.currentOpacity - .5125) < 1e-5);
    assert(fading.currentBlur > 0);
    assert(fabs(fading.ghostOpacity - .0125) < 1e-5);
    advance(.326);
    assert(EtchGPUNeedsPath() && current == fading.current);
    EtchGPUFrame *waiting = EtchGPUFrameState();
    assert(fabs(waiting.currentOpacity - .025) < 1e-6 && waiting.currentBlur > 0);
    assert(waiting.ghostOpacity == 0);
    EtchGPUPath *old = current; pending = fixturePath(.2f);
    EtchGPUTick(testClock, YES);
    assert(ghost == old && current != old && !pending);
    assert(fabs(ghostTime - .5) < 1e-9 && playhead == 0);
    EtchGPUFrame *swapped = EtchGPUFrameState();
    assert(fabs(swapped.ghostOpacity - .025) < 1e-6 && swapped.ghostBlur > 0);
    assert(swapped.ghost == old && fabs(swapped.ghostTime - .5) < 1e-9);
    assert(fading.current == old); /* Copied render state retains its immutable bundle. */
    EtchGPUShutdown(); assert(!current && !pending && !ghost);
}
