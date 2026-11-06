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
    needsPath = NO; /* Emulate the accepted planner offer before publication. */
    EtchGPUTick(testClock, YES);
    assert(ghost == old && current != old && !pending);
    assert(fabs(ghostTime - .5) < 1e-9 && playhead == 0);
    EtchGPUFrame *swapped = EtchGPUFrameState();
    assert(fabs(swapped.ghostOpacity - .025) < 1e-6 && swapped.ghostBlur > 0);
    assert(swapped.ghost == old && fabs(swapped.ghostTime - .5) < 1e-9);
    assert(fading.current == old); /* Copied render state retains its immutable bundle. */
    EtchGPUShutdown(); assert(!current && !pending && !ghost);
}

static void automaticHoldAndErase(void)
{
    startFixture(); playhead = current.duration;
    advance(.599);
    assert(!erasing && !EtchGPUNeedsPath());
    advance(.002);
    assert(erasing && fabs(eraseTime) < 1e-9);
    advance(.649);
    assert(!EtchGPUNeedsPath() && current);
    advance(.002);
    assert(EtchGPUNeedsPath() && current);
    EtchGPUFrame *frame = EtchGPUFrameState();
    assert(fabs(frame.currentTime - 2) < 1e-9);
    assert(fabs(frame.currentOpacity - .025) < 1e-6);
    EtchGPUPath *finished = current; pending = fixturePath(.2f);
    needsPath = NO;
    EtchGPUTick(testClock, YES);
    assert(ghost == finished && ghostTime == 2 && playhead == 0);
    assert(holdTime == 0 && eraseTime == 0 && !erasing && !EtchGPUNeedsPath());
    EtchGPUShutdown();
}

static id<MTLTexture> offscreenTexture(void)
{
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:320 height:240 mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared; descriptor.usage = MTLTextureUsageRenderTarget;
    id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor]; assert(texture);
    return texture;
}
static unsigned char *renderFrame(EtchGPUFrame *frame)
{
    id<MTLTexture> target = offscreenTexture();
    id<MTLCommandQueue> queue = [device newCommandQueue]; assert(queue);
    id<MTLCommandBuffer> command = [queue commandBuffer]; assert(command);
    assert(EtchGPUEncode(command, target, frame, backgroundPipeline, linePipeline, 1));
    [command commit]; [command waitUntilCompleted];
    assert(command.status == MTLCommandBufferStatusCompleted);
    unsigned char *pixels = malloc(320 * 240 * 4); assert(pixels);
    [target getBytes:pixels bytesPerRow:320 * 4 fromRegion:MTLRegionMake2D(0, 0, 320, 240)
          mipmapLevel:0];
    return pixels;
}
static double darkening(const unsigned char *background, const unsigned char *ink, int x, int y)
{
    size_t index = ((size_t)y * 320 + x) * 4;
    return ((int)background[index] - ink[index] + (int)background[index + 1] - ink[index + 1]
            + (int)background[index + 2] - ink[index + 2]) / 3.0;
}

static void ghostPixelsExcludeUndrawnGeometry(void)
{
    startFixture(); ghost = fixturePath(.15f); ghostTime = 2; advance(.5);
    unsigned char *background = renderFrame([EtchGPUFrame new]);
    unsigned char *before = renderFrame(EtchGPUFrameState());
    EtchGPUErase(); EtchGPUTick(testClock, YES); advance(.325);
    unsigned char *middle = renderFrame(EtchGPUFrameState());
    assert(darkening(background, before, 60, 60) > 100);
    assert(darkening(background, middle, 60, 60) > 30);
    assert(darkening(background, middle, 60, 60) < darkening(background, before, 60, 60));
    assert(darkening(background, before, 60, 36) >= 2);
    assert(darkening(background, middle, 60, 36) < darkening(background, before, 60, 36));
    advance(.326); pending = fixturePath(.2f); needsPath = NO; EtchGPUTick(testClock, YES);
    unsigned char *after = renderFrame(EtchGPUFrameState());
    double ghostDark = darkening(background, after, 60, 60);
    assert(ghostDark >= 2 && ghostDark <= 10); /* Pale 2.5% ink, rather than the old dark line. */
    assert(darkening(background, after, 60, 62) > 0); /* Soft fringe beyond the crisp radius. */
    assert(fabs(darkening(background, after, 110, 60)) < .1); /* Unfinished first stroke. */
    assert(fabs(darkening(background, after, 240, 180)) < .1); /* Unstarted second stroke. */
    assert(fabs(darkening(background, after, 60, 36)) < .1); /* Older ghost has faded away. */
    printf("Metal ghost: center %.3f/255, fringe %.3f/255; later geometry absent\n",
           ghostDark, darkening(background, after, 60, 62));
    free(background); free(before); free(middle); free(after);
    EtchGPUShutdown();
}

static void drainRetainsSubmittedResources(void)
{
    startFixture(); advance(.5); EtchGPUErase(); EtchGPUTick(testClock, YES); advance(.25);
    EtchGPUFrame *held = EtchGPUFrameState();
    id<MTLTexture> target = offscreenTexture();
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLCommandBuffer> command = [queue commandBuffer];
    assert(EtchGPUEncode(command, target, held, backgroundPipeline, linePipeline, 1));
    dispatch_group_t group = renderGroup;
    dispatch_semaphore_t gate = renderGate;
    assert(dispatch_semaphore_wait(gate, DISPATCH_TIME_NOW) == 0);
    dispatch_group_enter(group);
    [command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
        assert(completed.status == MTLCommandBufferStatusCompleted);
        assert(held.current.buffer && held.current.count == 2);
        EtchGPUFrameDone(gate, group);
    }];
    [command commit]; EtchGPUShutdown(); /* Production drain waits for real submitted Metal work. */
    assert(command.status == MTLCommandBufferStatusCompleted);
    assert(!current && !pending && !ghost && !renderQueue && !renderGroup && !device);
    assert(held.current.buffer && held.currentTime == .5);
    EtchGPUInit(MTLCreateSystemDefaultDevice(), 1200);
    EtchSnapshot snapshot = {calloc(1, sizeof(MotionSegment)), 1, 100, 100};
    assert(snapshot.segments); snapshot.segments[0] = (MotionSegment){{0, 0}, {100, 0}};
    assert(EtchGPUOfferSnapshot(&snapshot) && !snapshot.segments);
    EtchGPUShutdown(); /* Also drains a pending background plan without a window. */
    assert(!planning && !current && !pending && !ghost && !plannerQueue);
}

static void rateBoundsAndQueuedOrder(void)
{
    startFixture();
    for (int i = 0; i < 20; ++i) EtchGPUAction('+');
    EtchGPUTick(testClock, YES); assert(drawRate == 1024);
    assert([EtchGPUStatus() containsString:@"1024"]);
    for (int i = 0; i < 30; ++i) EtchGPUAction('-');
    EtchGPUTick(testClock, YES); assert(drawRate == 1.0 / 64);
    EtchGPUSetRate(1024); EtchGPUAction('+'); EtchGPUAction('-');
    EtchGPUTick(testClock, YES); assert(drawRate == 512);
    EtchGPUSetRate(1.0 / 64); EtchGPUAction('-'); EtchGPUAction('+');
    EtchGPUTick(testClock, YES); assert(drawRate == 1.0 / 32);
    EtchGPUAction('='); EtchGPUTick(testClock, YES); assert(drawRate == 1.0 / 16);
    EtchGPUAction('_'); EtchGPUTick(testClock, YES); assert(drawRate == 1.0 / 32);
    EtchGPUShutdown();
}

static void rateScalesDrawingHoldAndFade(void)
{
    startFixture(); EtchGPUSetRate(2); EtchGPUTick(testClock, YES);
    advance(.125); assert(fabs(playhead - .25) < 1e-9);
    playhead = current.duration; holdTime = 0;
    advance(.299); assert(!erasing && !EtchGPUNeedsPath());
    advance(.002); assert(erasing && eraseTime == 0);
    advance(.324); assert(!EtchGPUNeedsPath());
    advance(.002); assert(EtchGPUNeedsPath());
    EtchGPUShutdown();
    startFixture(); EtchGPUSetRate(1.0 / 64); EtchGPUTick(testClock, YES);
    advance(.5); assert(fabs(playhead - 1.0 / 128) < 1e-9);
    EtchGPUShutdown();
}

static void pauseAndManualErase(void)
{
    startFixture(); advance(.5);
    EtchGPUAction(KEY_F9); EtchGPUTick(testClock, YES);
    assert(paused); advance(.25); assert(fabs(playhead - .5) < 1e-9);
    EtchGPUAction(KEY_F7); EtchGPUTick(testClock, YES);
    assert(erasing && eraseTime == 0);
    advance(.25); assert(eraseTime == 0 && fabs(playhead - .5) < 1e-9);
    EtchGPUAction(KEY_F9); EtchGPUTick(testClock, YES); assert(!paused);
    advance(.325); assert(fabs(eraseTime - .325) < 1e-9);
    assert(fabs(playhead - .5) < 1e-9);
    EtchGPUShutdown();
}

static EtchGPUPath *publishInstantFixture(float y)
{
    assert(instant && EtchGPUNeedsPath() && !pending);
    EtchGPUPath *prior = current;
    pending = fixturePath(y); needsPath = NO; /* Accepted synthetic planner result. */
    EtchGPUTick(testClock, YES);
    assert(current != prior && playhead == current.duration);
    return prior;
}

static void instantGhostCadenceIgnoresDrawingRate(void)
{
    const double rates[] = {1.0 / 64, 1024};
    for (size_t i = 0; i < 2; ++i) {
        startFixture(); EtchGPUSetRate(rates[i]); EtchGPUSetInstant(YES);
        EtchGPUTick(testClock, YES);
        assert(instant && !paused && playhead == current.duration);
        EtchGPUPath *initial = publishInstantFixture(.2f);
        assert(ghost == initial && ghostTime == initial.duration);
        advance(.010); publishInstantFixture(.18f);
        assert(ghost == initial);
        advance(.022); publishInstantFixture(.16f);
        assert(ghost == initial); /* 32 ms remains below one 30 Hz interval at either rate. */
        advance(.002); EtchGPUPath *last = publishInstantFixture(.14f);
        assert(ghost == last && ghostTime == last.duration);
        assert(instantGhostElapsed < 1.0 / 30);
        EtchGPUShutdown();
    }
}

static void modeSwitchesClearPauseAndResetTimedDrawing(void)
{
    startFixture(); advance(.25);
    EtchGPUAction(KEY_F9); EtchGPUTick(testClock, YES); assert(paused);
    EtchGPUAction(KEY_F8); EtchGPUTick(testClock, YES);
    assert(instant && !paused && playhead == current.duration);
    publishInstantFixture(.2f);
    EtchGPUAction(KEY_F9); EtchGPUTick(testClock, YES); assert(paused);
    double elapsed = instantGhostElapsed;
    advance(.1); assert(instantGhostElapsed == elapsed && !EtchGPUNeedsPath());
    EtchGPUAction(KEY_F8); EtchGPUTick(testClock, YES);
    assert(!instant && !paused && EtchGPUNeedsPath());
    pending = fixturePath(.15f); needsPath = NO;
    EtchGPUTick(testClock, YES);
    assert(playhead == 0);
    advance(.125); assert(fabs(playhead - .125) < 1e-9);
    EtchGPUShutdown();
}

static void invalidationDiscardsInflightPlan(BOOL invalidGeometry)
{
    startFixture(); EtchGPUSetInstant(YES); EtchGPUTick(testClock, YES);
    ghost = fixturePath(.15f); ghostTime = 2;
    dispatch_semaphore_t entered = dispatch_semaphore_create(0);
    dispatch_semaphore_t unblock = dispatch_semaphore_create(0);
    dispatch_async(plannerQueue, ^{
        dispatch_semaphore_signal(entered);
        dispatch_semaphore_wait(unblock, DISPATCH_TIME_FOREVER);
    });
    assert(dispatch_semaphore_wait(entered, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC)) == 0);
    EtchSnapshot snapshot = {calloc(1, sizeof(MotionSegment)), 1, 100, 100};
    assert(snapshot.segments);
    snapshot.segments[0] = (MotionSegment){{0, 0}, {invalidGeometry ? NAN : 100, 0}};
    assert(EtchGPUOfferSnapshot(&snapshot) && !snapshot.segments && planning);
    uint64_t oldGeneration = generation;
    EtchGPUAction(KEY_F9); EtchGPUTick(testClock, YES); assert(paused);
    EtchGPUInvalidate(); EtchGPUTick(testClock, YES);
    assert(generation == oldGeneration + 1 && !current && !pending && !ghost && paused);
    assert(planning && !EtchGPUNeedsPath()); /* Bound work until the old worker drains. */
    dispatch_semaphore_signal(unblock); dispatch_sync(plannerQueue, ^{});
    assert(!planning && !current && !pending && !ghost && !EtchGPUNeedsPath());
    EtchGPUAction(KEY_F9); EtchGPUTick(testClock, YES);
    assert(!paused && EtchGPUNeedsPath());
    assert(EtchGPUUploadCount() == 0);
    const char *error = EtchGPUError(); assert(!error || !*error);
    EtchGPUShutdown();
}

int main(void)
{
    @autoreleasepool {
        if (!MTLCreateSystemDefaultDevice()) {
            fputs("Metal device required: enable this test only with DOOM_ENABLE_GPU_TESTS\n", stderr);
            return EXIT_FAILURE;
        }
        manualEraseFreezesRevealedPrefix(); automaticHoldAndErase();
        ghostPixelsExcludeUndrawnGeometry(); drainRetainsSubmittedResources();
        rateBoundsAndQueuedOrder(); rateScalesDrawingHoldAndFade(); pauseAndManualErase();
        instantGhostCadenceIgnoresDrawingRate(); modeSwitchesClearPauseAndResetTimedDrawing();
        invalidationDiscardsInflightPlan(NO); invalidationDiscardsInflightPlan(YES);
        puts("GPU ghost timing, prefix clipping, Metal pixels and resource lifetimes passed");
    }
    return EXIT_SUCCESS;
}
