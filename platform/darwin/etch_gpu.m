#include "rate.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_gpu.h"
#include "timeline.h"
#include "doomdef.h"
#include <stdint.h>
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

/* Every queued render owns its two bundles and a copy of playback uniforms. */
@interface EtchGPUFrame : NSObject
@property(nonatomic, strong) EtchGPUPath *current;
@property(nonatomic, strong) EtchGPUPath *ghost;
@property(nonatomic) double currentTime, ghostTime;
@property(nonatomic) float currentOpacity, currentBlur, ghostOpacity, ghostBlur;
@end
@implementation EtchGPUFrame
@end

static os_unfair_lock stateLock = OS_UNFAIR_LOCK_INIT;
static id<MTLDevice> device;
static id<MTLRenderPipelineState> linePipeline, backgroundPipeline;
static id<MTLTexture> readTarget;
static id<MTLBuffer> readBuffer;
static EtchGPUPath *current, *pending, *ghost;
static double ghostTime, holdTime, eraseTime;
static BOOL erasing;
static BOOL instant, paused, replaceRequested, controlClockReset;
static double drawRate = 1, instantGhostElapsed;
static const double ghostInterval = 1.0 / 30;
static uint64_t generation;
static EtchSnapshot latestSnapshot;
static uint64_t latestGeneration;
enum { EtchRate, EtchInstant, EtchAction, EtchInvalidate, CONTROL_CAPACITY = 256 };
typedef struct { int kind; double value; } EtchGPUControl;
static EtchGPUControl controls[CONTROL_CAPACITY];
static atomic_uint controlRead, controlWrite;
static atomic_bool controlOverflow;
static const double holdSeconds = .6, eraseSeconds = .65, ghostOpacity = .025;
static dispatch_queue_t plannerQueue, renderQueue;
static dispatch_semaphore_t renderGate;
static dispatch_group_t renderGroup;
static atomic_uint frames, uploads;
static char failure[256];
static BOOL running, planning, needsPath, clockStarted;
static double speed, previous, playhead;

static void EtchGPUFreeLatest(void)
{
    free(latestSnapshot.segments);
    latestSnapshot = (EtchSnapshot){0};
}

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
    "struct Uniforms { float2 size; float radius; float time; float opacity; float blur; float2 pad; };\n"
    "struct Vertex { float4 pos [[position]]; float2 p; float len [[flat]]; float visible [[flat]]; };\n"
    "vertex Vertex lineVertex(uint v [[vertex_id]], uint i [[instance_id]],\n"
    "    constant Segment *segments [[buffer(0)]], constant Uniforms &u [[buffer(1)]]) {\n"
    "  Segment s = segments[i]; float dt = s.timing.y-s.timing.x;\n"
    "  float f = dt>0 ? clamp((u.time-s.timing.x)/dt,0.0,1.0) : step(s.timing.y,u.time);\n"
    "  float2 a=s.ends.xy*u.size, b=s.ends.zw*u.size, delta=b-a;\n"
    "  float full=length(delta); float2 dir=full>0 ? delta/full : float2(1,0);\n"
    "  float2 normal=float2(-dir.y,dir.x); float len=full*f, r=u.radius+1.5+u.blur*3.0;\n"
    "  float2 corners[6]={float2(0,-1),float2(1,-1),float2(0,1),float2(0,1),float2(1,-1),float2(1,1)};\n"
    "  float2 q=float2(mix(-r,len+r,corners[v].x),corners[v].y*r);\n"
    "  float2 p=a+dir*q.x+normal*q.y; Vertex out;\n"
    "  out.pos=float4(p.x/u.size.x*2-1,1-p.y/u.size.y*2,0,1);\n"
    "  out.p=q; out.len=len; out.visible=(f>0 && full>0) ? 1 : 0; return out;\n"
    "}\n"
    "fragment float4 lineFragment(Vertex in [[stage_in]], constant Uniforms &u [[buffer(0)]]) {\n"
    "  if(in.visible==0) discard_fragment();\n"
    "  float dist=length(float2(in.p.x-clamp(in.p.x,0.0,in.len),in.p.y));\n"
    "  float aa=max(fwidth(dist),.65);\n"
    "  float coverage=1-smoothstep(u.radius-aa*.5,u.radius+aa*.5,dist);\n"
    "  if(u.blur>0) { float outside=max(0.0,dist-u.radius);\n"
    "    coverage=exp(-.5*outside*outside/(u.blur*u.blur)); }\n"
    "  float grain=fract(sin(dot(in.pos.xy,float2(12.9898,78.233)))*43758.5453);\n"
    "  return float4(float3(.12,.135,.12)+grain*.025,coverage*u.opacity);\n"
    "}\n"
    "struct Background { float4 pos [[position]]; float2 uv; };\n"
    "vertex Background bgVertex(uint i [[vertex_id]]) {\n"
    "  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; Background out;\n"
    "  out.pos=float4(p[i],0,1); out.uv=p[i]*.5+.5; return out;\n"
    "}\n"
    "fragment float4 bgFragment(Background in [[stage_in]]) {\n"
    "  float grain=fract(sin(dot(in.pos.xy,float2(12.9898,78.233)))*43758.5453);\n"
    "  float edge=dot(in.uv-.5,in.uv-.5);\n"
    "  return float4(float3(.89,.875,.825)+(grain-.5)*.018-edge*.028,1);\n"
    "}\n";

static BOOL EtchGPUPipelines(void)
{
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:EtchGPUShader options:nil error:&error];
    if (!library) { EtchGPUFail(error.localizedDescription.UTF8String ?: "Cannot compile Metal shader"); return NO; }
    for (int line = 0; line < 2; line++) {
        MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];
        descriptor.vertexFunction = [library newFunctionWithName:line ? @"lineVertex" : @"bgVertex"];
        descriptor.fragmentFunction = [library newFunctionWithName:line ? @"lineFragment" : @"bgFragment"];
        MTLRenderPipelineColorAttachmentDescriptor *color = descriptor.colorAttachments[0];
        color.pixelFormat = MTLPixelFormatBGRA8Unorm;
        color.blendingEnabled = line;
        color.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        color.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        color.sourceAlphaBlendFactor = MTLBlendFactorOne;
        color.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if (!pipeline) { EtchGPUFail(error.localizedDescription.UTF8String ?: "Cannot create Metal pipeline"); return NO; }
        if (line) linePipeline = pipeline;
        else backgroundPipeline = pipeline;
    }
    return YES;
}

void EtchGPUInit(id<MTLDevice> gpu, double visual_speed_mm_s)
{
    EtchGPUShutdown();
    failure[0] = 0; atomic_store(&frames, 0); atomic_store(&uploads, 0);
    if (!gpu || !isfinite(visual_speed_mm_s) || visual_speed_mm_s <= 0) {
        EtchGPUFail("Metal playback requires a device and a positive finite drawing speed"); return;
    }
    device = gpu; speed = visual_speed_mm_s;
    if (!EtchGPUPipelines()) return;
    plannerQueue = dispatch_queue_create("doom-etch.planner", DISPATCH_QUEUE_SERIAL);
    renderQueue = dispatch_queue_create("doom-etch.renderer", DISPATCH_QUEUE_SERIAL);
    renderGate = dispatch_semaphore_create(1); renderGroup = dispatch_group_create();
    running = needsPath = YES;
}

void EtchGPUShutdown(void)
{
    os_unfair_lock_lock(&stateLock);
    running = needsPath = NO;
    EtchGPUFreeLatest();
    os_unfair_lock_unlock(&stateLock);
    // Draining is allowed during shutdown, never during normal frame submission.
    if (plannerQueue) dispatch_sync(plannerQueue, ^{});
    if (renderQueue) dispatch_sync(renderQueue, ^{});
    if (renderGroup) dispatch_group_wait(renderGroup, DISPATCH_TIME_FOREVER);
    os_unfair_lock_lock(&stateLock);
    current = pending = ghost = nil;
    ghostTime = holdTime = eraseTime = 0; erasing = NO;
    generation++;
    instant = paused = replaceRequested = controlClockReset = NO;
    drawRate = 1; instantGhostElapsed = ghostInterval;
    atomic_store(&controlRead, 0); atomic_store(&controlWrite, 0);
    atomic_store(&controlOverflow, NO);
    planning = clockStarted = NO;
    playhead = previous = 0;
    os_unfair_lock_unlock(&stateLock);
    plannerQueue = renderQueue = nil; renderGate = nil; renderGroup = nil;
    linePipeline = backgroundPipeline = nil; readTarget = nil; readBuffer = nil; device = nil;
}

static EtchGPUPath *EtchGPUBuildPath(EtchSnapshot snapshot, id<MTLDevice> gpu, double drawingSpeed, const char **message)
{
    MotionPath path = {0};
    int planned = MotionPlanSegments(&path, snapshot.segments, snapshot.count,
        snapshot.width, snapshot.height, (MotionPoint){0, 0});
    free(snapshot.segments);
    if (!planned) { *message = "Cannot plan captured wall and artwork segments"; return nil; }
    MotionTimedSegment *data = NULL; size_t count = 0; double seconds = 0;
    int timed = MotionTimelineBuild(&path, drawingSpeed, &data, &count, &seconds);
    MotionFree(&path);
    if (!timed) { *message = "Cannot construct the captured path timeline"; return nil; }
    EtchGPUPath *bundle = [EtchGPUPath new];
    if (count && count <= gpu.maxBufferLength / sizeof(*data))
        bundle.buffer = [gpu newBufferWithBytes:data length:count * sizeof(*data) options:MTLResourceStorageModeShared];
    free(data);
    if (count && !bundle.buffer) { *message = "Cannot allocate the immutable timed path buffer"; return nil; }
    bundle.count = count; bundle.duration = seconds;
    return bundle;
}

static void EtchGPUQueueControl(int kind, double value)
{
    unsigned write = atomic_load(&controlWrite), read = atomic_load(&controlRead);
    if (write - read >= CONTROL_CAPACITY) { atomic_store(&controlOverflow, YES); return; }
    controls[write % CONTROL_CAPACITY] = (EtchGPUControl){kind, value};
    atomic_store(&controlWrite, write + 1);
}
void EtchGPUSetRate(double rate)
{
    if (isfinite(rate) && rate > 0) EtchGPUQueueControl(EtchRate, rate);
}
void EtchGPUSetInstant(BOOL enabled) { EtchGPUQueueControl(EtchInstant, enabled != NO); }
void EtchGPUInvalidate(void) { EtchGPUQueueControl(EtchInvalidate, 0); }

static void EtchGPUChangeMode(BOOL enabled)
{
    instant = enabled; paused = NO; erasing = NO;
    if (!instant) EtchGPUFreeLatest();
    holdTime = eraseTime = 0; instantGhostElapsed = ghostInterval;
    if (instant && current) playhead = current.duration;
    needsPath = replaceRequested = controlClockReset = YES;
}
static void EtchGPUClearPaths(void)
{
    generation++;
    EtchGPUFreeLatest();
    current = pending = ghost = nil;
    playhead = ghostTime = holdTime = eraseTime = 0;
    erasing = replaceRequested = NO;
    instantGhostElapsed = ghostInterval;
    needsPath = controlClockReset = YES;
    // Keep planning true until the one old worker finishes and discards its result.
}

static void EtchGPUApplyControl(EtchGPUControl control)
{
    if (control.kind == EtchRate) drawRate = EtchVisualRateClamp(control.value);
    else if (control.kind == EtchInstant) EtchGPUChangeMode(control.value != 0);
    else if (control.kind == EtchInvalidate) EtchGPUClearPaths();
    else {
        int key = (int)control.value;
        drawRate = EtchVisualRateKey(drawRate, key);
        if (key == KEY_F8) EtchGPUChangeMode(!instant);
        if (key == KEY_F9) { paused = !paused; if (paused) EtchGPUFreeLatest(); }
        if (key == KEY_F7 && current && !instant && !erasing) {
            erasing = YES; eraseTime = holdTime = 0; controlClockReset = YES;
        }
    }
}
static void EtchGPUControls(void)
{
    if (atomic_exchange(&controlOverflow, NO) && !failure[0])
        snprintf(failure, sizeof(failure), "GPU input queue exceeded its 256-action bound");
    unsigned read = atomic_load(&controlRead), write = atomic_load(&controlWrite);
    while (read != write) EtchGPUApplyControl(controls[(read++) % CONTROL_CAPACITY]);
    atomic_store(&controlRead, read);
}

static void EtchGPUPlanSnapshots(EtchSnapshot snapshot, uint64_t epoch,
                                 id<MTLDevice> gpu, double drawingSpeed)
{
    for (;;) @autoreleasepool {
        const char *message = NULL;
        EtchGPUPath *bundle = EtchGPUBuildPath(snapshot, gpu, drawingSpeed, &message);
        os_unfair_lock_lock(&stateLock);
        if (running && epoch == generation) {
            if (bundle) { pending = bundle; atomic_fetch_add(&uploads, 1); }
            else if (message && !failure[0]) snprintf(failure, sizeof(failure), "%s", message);
        }
        BOOL next = running && instant && !paused && !failure[0] && latestSnapshot.width;
        if (next) {
            snapshot = latestSnapshot; epoch = latestGeneration;
            latestSnapshot = (EtchSnapshot){0};
        } else {
            EtchGPUFreeLatest(); planning = NO;
        }
        os_unfair_lock_unlock(&stateLock);
        if (!next) return;
    }
}

BOOL EtchGPUOfferSnapshot(EtchSnapshot *snapshot)
{
    if (!snapshot || (!snapshot->segments && snapshot->count) || snapshot->count > 400000
        || !snapshot->width || !snapshot->height) return NO;
    if (!os_unfair_lock_trylock(&stateLock)) return NO;
    EtchGPUControls();
    if (!running || paused || failure[0] || (!instant && (!needsPath || planning || pending))) {
        os_unfair_lock_unlock(&stateLock); return NO;
    }
    if (planning) {
        EtchGPUFreeLatest();
        latestSnapshot = *snapshot; latestGeneration = generation;
        *snapshot = (EtchSnapshot){0};
        os_unfair_lock_unlock(&stateLock); return YES;
    }
    EtchSnapshot transferred = *snapshot;
    *snapshot = (EtchSnapshot){0};
    planning = YES; needsPath = NO;
    id<MTLDevice> gpu = device;
    double drawingSpeed = speed;
    uint64_t acceptedGeneration = generation;
    dispatch_queue_t queue = plannerQueue;
    os_unfair_lock_unlock(&stateLock);
    dispatch_async(queue, ^{
        EtchGPUPlanSnapshots(transferred, acceptedGeneration, gpu, drawingSpeed);
    });
    return YES;
}

BOOL EtchGPUNeedsPath(void)
{
    if (!os_unfair_lock_trylock(&stateLock)) return NO;
    EtchGPUControls();
    BOOL requested = running && !paused && !failure[0]
        && (instant || (needsPath && !planning && !pending));
    os_unfair_lock_unlock(&stateLock);
    return requested;
}

void EtchGPUAction(int key)
{
    if (key == '+' || key == '=' || key == '-' || key == '_'
        || key == KEY_F7 || key == KEY_F8 || key == KEY_F9)
        EtchGPUQueueControl(EtchAction, key);
}
void EtchGPUErase(void) { EtchGPUAction(KEY_F7); }

static void EtchGPUAdvance(double dt)
{
    if (erasing) {
        eraseTime = fmin(eraseSeconds, eraseTime + dt);
        if (eraseTime + 1e-9 >= eraseSeconds) {
            eraseTime = eraseSeconds;
            if (!planning && !pending) needsPath = YES;
        }
    } else if (playhead < current.duration) {
        playhead = fmin(current.duration, playhead + dt);
    } else {
        holdTime += dt;
        if (holdTime + 1e-9 >= holdSeconds) {
            erasing = YES; eraseTime = 0;
        }
    }
}

static void EtchGPUInstallPending(void)
{
    if (!instant || instantGhostElapsed + 1e-9 >= ghostInterval) {
        ghost = current;
        ghostTime = current ? fmin(playhead, current.duration) : 0;
        if (instant) instantGhostElapsed = fmod(instantGhostElapsed, ghostInterval);
    }
    current = pending; pending = nil;
    playhead = instant ? current.duration : 0;
    holdTime = eraseTime = 0; erasing = NO;
    needsPath = replaceRequested = NO;
}

void EtchGPUTick(double now, BOOL active)
{
    if (!isfinite(now) || !os_unfair_lock_trylock(&stateLock)) return;
    if (!running) { os_unfair_lock_unlock(&stateLock); return; }
    EtchGPUControls();
    double dt = clockStarted ? now - previous : 0;
    previous = now; clockStarted = YES;
    BOOL replaced = controlClockReset; controlClockReset = NO;
    BOOL advanceClock = active && !paused && !replaced && dt >= 0 && dt <= .5;
    if (instant && advanceClock) instantGhostElapsed += dt;
    if (pending && !paused && (!current || instant || replaceRequested
            || (erasing && eraseTime >= eraseSeconds))) {
        EtchGPUInstallPending(); replaced = YES;
    }
    if (advanceClock && current && !replaced && !instant) EtchGPUAdvance(dt * drawRate);
    if (instant && active && !paused && !planning && !pending) needsPath = YES;
    os_unfair_lock_unlock(&stateLock);
}

static EtchGPUFrame *EtchGPUFrameState(void)
{
    double fade = erasing ? fmin(1, eraseTime / eraseSeconds) : 0;
    EtchGPUFrame *frame = [EtchGPUFrame new];
    frame.current = current; frame.ghost = ghost;
    frame.currentTime = playhead; frame.ghostTime = ghostTime;
    frame.currentOpacity = (float)(1 - (1 - ghostOpacity) * fade);
    frame.currentBlur = (float)(1.3 * fade);
    frame.ghostOpacity = (float)(ghostOpacity * (1 - fade));
    frame.ghostBlur = 1.3f;
    return frame;
}

static void EtchGPUEncodeLayer(id<MTLRenderCommandEncoder> encoder, EtchGPUPath *path,
    double clock, float opacity, float blur, double width, double height, double backingScale)
{
    if (!path.buffer || !path.count || opacity <= 0) return;
    float uniforms[8] = {(float)width, (float)height, (float)(.65 * backingScale),
        (float)clock, opacity, (float)(blur * backingScale), 0, 0};
    [encoder setVertexBuffer:path.buffer offset:0 atIndex:0];
    [encoder setVertexBytes:uniforms length:sizeof(uniforms) atIndex:1];
    [encoder setFragmentBytes:uniforms length:sizeof(uniforms) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:path.count];
}

static BOOL EtchGPUEncode(id<MTLCommandBuffer> command, id<MTLTexture> target,
    EtchGPUFrame *frame, id<MTLRenderPipelineState> background, id<MTLRenderPipelineState> lines,
    double backingScale)
{
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(.12, .1, .09, 1);
    id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
    if (!encoder) { EtchGPUFail("Cannot create the GPU drawing encoder"); return NO; }
    double width = target.width, height = width * .75;
    if (height > target.height) { height = target.height; width = height * 4 / 3; }
    [encoder setViewport:(MTLViewport){(target.width - width) / 2,
        (target.height - height) / 2, width, height, 0, 1}];
    [encoder setRenderPipelineState:background];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder setRenderPipelineState:lines];
    EtchGPUEncodeLayer(encoder, frame.ghost, frame.ghostTime, frame.ghostOpacity,
        frame.ghostBlur, width, height, backingScale);
    EtchGPUEncodeLayer(encoder, frame.current, frame.currentTime, frame.currentOpacity,
        frame.currentBlur, width, height, backingScale);
    [encoder endEncoding];
    return YES;
}

static void EtchGPUFrameDone(dispatch_semaphore_t gate, dispatch_group_t group)
{
    dispatch_semaphore_signal(gate);
    dispatch_group_leave(group);
}

static void EtchGPURender(id<MTLCommandQueue> queue, CAMetalLayer *layer,
    EtchGPUFrame *frame, id<MTLRenderPipelineState> background, id<MTLRenderPipelineState> lines,
    double backingScale, dispatch_semaphore_t gate, dispatch_group_t group)
{
    @autoreleasepool {
        // Drawable acquisition can wait for WindowServer, so it stays on this queue.
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if (!drawable) { EtchGPUFrameDone(gate, group); return; }
        id<MTLCommandBuffer> command = [queue commandBuffer];
        if (!command) {
            EtchGPUFail("Cannot create a GPU drawing command buffer");
            EtchGPUFrameDone(gate, group); return;
        }
        if (!EtchGPUEncode(command, drawable.texture, frame, background, lines, backingScale)) {
            EtchGPUFrameDone(gate, group); return;
        }
        NSUInteger count = frame.current.count, ghostCount = frame.ghost.count;
        [command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
            // Both bundles and their copied uniforms remain alive through completion.
            if (frame.current.count != count || frame.ghost.count != ghostCount)
                EtchGPUFail("A submitted drawing path was modified");
            if (completed.status == MTLCommandBufferStatusCompleted) atomic_fetch_add(&frames, 1);
            else EtchGPUFail(completed.error.localizedDescription.UTF8String ?: "GPU drawing did not complete");
            EtchGPUFrameDone(gate, group);
        }];
        [command presentDrawable:drawable];
        [command commit];
    }
}

BOOL EtchGPUDraw(id<MTLCommandQueue> queue, CAMetalLayer *layer, double backingScale, unsigned frameLimit)
{
    if (!queue || !layer || !isfinite(backingScale) || backingScale <= 0 || backingScale > 1e6)
        return NO;
    dispatch_semaphore_t gate = renderGate;
    if (!gate || dispatch_semaphore_wait(gate, DISPATCH_TIME_NOW) != 0) return NO;
    // Completion can advance the count between the caller's check and this gate.
    if (frameLimit && atomic_load(&frames) >= frameLimit) {
        dispatch_semaphore_signal(gate); return NO;
    }
    if (!os_unfair_lock_trylock(&stateLock)) { dispatch_semaphore_signal(gate); return NO; }
    EtchGPUControls();
    if (!running || failure[0]) {
        os_unfair_lock_unlock(&stateLock); dispatch_semaphore_signal(gate); return NO;
    }
    EtchGPUFrame *frame = EtchGPUFrameState();
    id<MTLRenderPipelineState> background = backgroundPipeline, lines = linePipeline;
    dispatch_queue_t renderer = renderQueue;
    dispatch_group_t group = renderGroup;
    dispatch_group_enter(group);
    os_unfair_lock_unlock(&stateLock);
    dispatch_async(renderer, ^{
        EtchGPURender(queue, layer, frame, background, lines, backingScale, gate, group);
    });
    return YES;
}

static EtchGPUPath *EtchGPUStrokePath(id<MTLDevice> gpu, EtchGPUStrokes strokes)
{
    EtchGPUPath *path = [EtchGPUPath new];
    if (strokes.count && strokes.segments && strokes.count <= gpu.maxBufferLength / sizeof(*strokes.segments))
        path.buffer = [gpu newBufferWithBytes:strokes.segments
            length:strokes.count * sizeof(*strokes.segments) options:MTLResourceStorageModeShared];
    path.count = path.buffer ? strokes.count : 0;
    return path;
}

int EtchGPUDrawStrokes(id<MTLCommandQueue> queue, CAMetalLayer *layer, double backingScale,
    EtchGPUStrokes ghost, EtchGPUStrokes current, double clock)
{
    if (!queue || !layer || !isfinite(clock) || !isfinite(backingScale) || backingScale <= 0) return -1;
    @autoreleasepool {
        if (!linePipeline) {
            device = queue.device;
            if (!EtchGPUPipelines()) return -1;
        }
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if (!drawable) return 0;
        EtchGPUFrame *frame = [EtchGPUFrame new];
        frame.ghost = EtchGPUStrokePath(queue.device, ghost);
        frame.current = EtchGPUStrokePath(queue.device, current);
        frame.ghostTime = frame.currentTime = clock;
        frame.ghostOpacity = ghost.opacity; frame.ghostBlur = ghost.blur;
        frame.currentOpacity = current.opacity; frame.currentBlur = current.blur;
        id<MTLCommandBuffer> command = [queue commandBuffer];
        if (!command || !EtchGPUEncode(command, drawable.texture, frame,
                backgroundPipeline, linePipeline, backingScale)) return -1;
        [command presentDrawable:drawable];
        [command commit];
        [command waitUntilCompleted];
        return command.status == MTLCommandBufferStatusCompleted ? 1 : -1;
    }
}

NSString *EtchGPUStatus(void)
{
    static NSString *cached = @"1× · timed";
    if (!os_unfair_lock_trylock(&stateLock)) return cached;
    EtchGPUControls();
    double rate = drawRate;
    NSString *mode = paused ? @"paused" : instant ? @"instant" : erasing ? @"erasing" : @"timed";
    os_unfair_lock_unlock(&stateLock);
    char text[32];
    EtchVisualRateText(rate, text, sizeof(text));
    cached = [NSString stringWithFormat:@"%@ · %@", @(text), mode];
    return cached;
}

static void EtchGPUReadStorage(id<MTLCommandQueue> queue)
{
    if (readTarget) return;
    MTLTextureDescriptor *desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
        MTLPixelFormatBGRA8Unorm width:768 height:576 mipmapped:NO];
    desc.usage=MTLTextureUsageRenderTarget; desc.storageMode=MTLStorageModePrivate;
    readTarget=[queue.device newTextureWithDescriptor:desc];
    readBuffer=[queue.device newBufferWithLength:768*576*4 options:MTLResourceStorageModeShared];
}
static void EtchGPUReadRender(id<MTLCommandQueue> queue,EtchGPUFrame *frame,
    id<MTLRenderPipelineState> background,id<MTLRenderPipelineState> lines,
    dispatch_semaphore_t gate,dispatch_group_t group,void (^completion)(NSData *))
{
    @autoreleasepool {
        EtchGPUReadStorage(queue);
        id<MTLTexture> target=readTarget;
        id<MTLBuffer> buffer=readBuffer;
        id<MTLCommandBuffer> command=[queue commandBuffer];
        if (!target || !buffer || !command || !EtchGPUEncode(command,target,frame,background,lines,1.5)) {
            EtchGPUFail("Cannot encode the rig display readback");
            EtchGPUFrameDone(gate,group); completion(nil); return;
        }
        id<MTLBlitCommandEncoder> copy=[command blitCommandEncoder];
        if (!copy) {
            EtchGPUFail("Cannot copy the rig display readback");
            EtchGPUFrameDone(gate,group); completion(nil); return;
        }
        [copy copyFromTexture:target sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(768,576,1) toBuffer:buffer destinationOffset:0
            destinationBytesPerRow:768*4 destinationBytesPerImage:768*576*4];
        [copy endEncoding];
        [command addCompletedHandler:^(id<MTLCommandBuffer> finished) {
            NSData *pixels=nil;
            if (finished.status==MTLCommandBufferStatusCompleted && target.width && background && lines) {
                (void)frame.current.buffer; (void)frame.ghost.buffer;
                pixels=[NSData dataWithBytes:buffer.contents length:768*576*4];
            } else EtchGPUFail("Rig display readback did not complete");
            EtchGPUFrameDone(gate,group);
            completion(pixels);
        }];
        [command commit];
    }
}
BOOL EtchGPUReadPixels(id<MTLCommandQueue> queue,void (^completion)(NSData *))
{
    if (!queue || !completion) return NO;
    dispatch_semaphore_t gate=renderGate;
    if (!gate || dispatch_semaphore_wait(gate,DISPATCH_TIME_NOW)!=0) return NO;
    if (!os_unfair_lock_trylock(&stateLock)) { dispatch_semaphore_signal(gate); return NO; }
    EtchGPUControls();
    if (!running || failure[0]) {
        os_unfair_lock_unlock(&stateLock); dispatch_semaphore_signal(gate); return NO;
    }
    EtchGPUFrame *frame=EtchGPUFrameState();
    id<MTLRenderPipelineState> background=backgroundPipeline,lines=linePipeline;
    dispatch_group_t group=renderGroup; dispatch_queue_t renderer=renderQueue;
    dispatch_group_enter(group);
    os_unfair_lock_unlock(&stateLock);
    dispatch_async(renderer,^{ EtchGPUReadRender(queue,frame,background,lines,gate,group,completion); });
    return YES;
}
