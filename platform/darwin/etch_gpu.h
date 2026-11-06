// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "r_etch.h"

void EtchGPUInit(id<MTLDevice> device, double visual_speed_mm_s);
void EtchGPUShutdown(void);
void EtchGPUTick(double now, BOOL active);
BOOL EtchGPUNeedsPath(void);
/* YES transfers and clears the snapshot; NO leaves ownership with the caller. */
BOOL EtchGPUOfferSnapshot(EtchSnapshot *snapshot);
/* Nonblocking main-thread submission; drawable acquisition runs on a worker. */
BOOL EtchGPUDraw(id<MTLCommandQueue> queue, CAMetalLayer *layer, double backingScale, unsigned frameLimit);
unsigned EtchGPUFrameCount(void);
unsigned EtchGPUUploadCount(void);
/* NULL when no error is ready; returned text lasts until this thread's next call. */
const char *EtchGPUError(void);
