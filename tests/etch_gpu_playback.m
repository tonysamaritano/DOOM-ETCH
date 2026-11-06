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
