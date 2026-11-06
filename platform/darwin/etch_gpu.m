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

static void EtchGPUFail(const char *message)
{
    os_unfair_lock_lock(&stateLock);
    if (!failure[0]) snprintf(failure, sizeof(failure), "%s", message);
    os_unfair_lock_unlock(&stateLock);
}
const char *EtchGPUError(void)
{
    static _Thread_local char copy[256];
    if (!os_unfair_lock_trylock(&stateLock)) return NULL;
    memcpy(copy, failure, sizeof(copy));
    os_unfair_lock_unlock(&stateLock);
    return copy[0] ? copy : NULL;
}
unsigned EtchGPUFrameCount(void) { return atomic_load(&frames); }
unsigned EtchGPUUploadCount(void) { return atomic_load(&uploads); }

static NSString *const EtchGPUShader =
    @"#include <metal_stdlib>\nusing namespace metal;\n"
    "struct Segment { float4 ends; float4 timing; };\n"
    "struct Uniforms { float2 size; float radius; float time; };\n"
    "struct Vertex { float4 pos [[position]]; float2 p; float len [[flat]]; float visible [[flat]]; };\n"
    "vertex Vertex lineVertex(uint v [[vertex_id]], uint i [[instance_id]],\n"
    "    constant Segment *segments [[buffer(0)]], constant Uniforms &u [[buffer(1)]]) {\n"
    "  Segment s = segments[i]; float dt = s.timing.y-s.timing.x;\n"
    "  float f = dt>0 ? clamp((u.time-s.timing.x)/dt,0.0,1.0) : step(s.timing.y,u.time);\n"
    "  float2 a=s.ends.xy*u.size, b=s.ends.zw*u.size, delta=b-a;\n"
    "  float full=length(delta); float2 dir=full>0 ? delta/full : float2(1,0);\n"
    "  float2 normal=float2(-dir.y,dir.x); float len=full*f, r=u.radius+1.5;\n"
    "  float2 corners[6]={float2(0,-1),float2(1,-1),float2(0,1),float2(0,1),float2(1,-1),float2(1,1)};\n"
    "  float2 q=float2(mix(-r,len+r,corners[v].x),corners[v].y*r);\n"
    "  float2 p=a+dir*q.x+normal*q.y; Vertex out;\n"
    "  out.pos=float4(p.x/u.size.x*2-1,1-p.y/u.size.y*2,0,1);\n"
    "  out.p=q; out.len=len; out.visible=(f>0 && full>0) ? 1 : 0; return out;\n"
    "}\n";
