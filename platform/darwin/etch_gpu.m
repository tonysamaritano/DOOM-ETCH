// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_gpu.h"
#include "timeline.h"
#include <dispatch/dispatch.h>
#include <os/lock.h>
#include <stdatomic.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Bundle fields are populated on the planner queue, then never modified. */
@interface EtchGPUPath : NSObject
@property(nonatomic, strong) id<MTLBuffer> buffer;
@property(nonatomic) NSUInteger count;
@property(nonatomic) double duration;
@end
@implementation EtchGPUPath
@end

static os_unfair_lock stateLock = OS_UNFAIR_LOCK_INIT;
static id<MTLDevice> device;
static id<MTLRenderPipelineState> linePipeline, backgroundPipeline;
static EtchGPUPath *current, *pending;
static dispatch_queue_t plannerQueue, renderQueue;
static dispatch_semaphore_t renderGate;
static dispatch_group_t renderGroup;
static atomic_uint frames, uploads;
static char failure[256];
static BOOL running, planning, needsPath, clockStarted;
static double speed, previous, playhead;
