// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "r_etch.h"
#include "timeline.h"

/* Caller-owned timed strokes for one layer; normalized ends, times in seconds. */
typedef struct {
    const MotionTimedSegment *segments;
    size_t count;
    float opacity, blur;
} EtchGPUStrokes;

void EtchGPUInit(id<MTLDevice> device, double visual_speed_mm_s);
void EtchGPUShutdown(void);
void EtchGPUTick(double now, BOOL active);
void EtchGPUErase(void);
/* Main-thread controls are queued and applied in order at the next playback entry. */
void EtchGPUSetRate(double rate);
void EtchGPUSetInstant(BOOL enabled);
void EtchGPUAction(int key);
void EtchGPUInvalidate(void);
NSString *EtchGPUStatus(void);
BOOL EtchGPUNeedsPath(void);
/* YES transfers and clears the snapshot; NO leaves ownership with the caller. */
BOOL EtchGPUOfferSnapshot(EtchSnapshot *snapshot);
/* Nonblocking main-thread submission; drawable acquisition runs on a worker. */
BOOL EtchGPUDraw(id<MTLCommandQueue> queue, CAMetalLayer *layer, double backingScale, unsigned frameLimit);
unsigned EtchGPUFrameCount(void);
unsigned EtchGPUUploadCount(void);
/* NULL when no error is ready; returned text lasts until this thread's next call. */
const char *EtchGPUError(void);
/* Bounded offscreen display readback. Completion owns immutable packed BGRA
 * pixels and runs off-main after render admission is released; nil on failure. */
/* Synchronous draw of G-code playback strokes with the same line shader.
 * Returns 1 when presented, 0 when no drawable was ready, -1 on failure. */
int EtchGPUDrawStrokes(id<MTLCommandQueue> queue, CAMetalLayer *layer, double backingScale,
    EtchGPUStrokes ghost, EtchGPUStrokes current, double clock);
BOOL EtchGPUReadPixels(id<MTLCommandQueue> queue, void (^completion)(NSData *pixels));
