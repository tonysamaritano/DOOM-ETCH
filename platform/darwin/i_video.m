// SPDX-License-Identifier: GPL-2.0-or-later
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import "keyboard.h"
#import "etch_gpu.h"
#import "etch_play.h"
#include "live.h"
#import <QuartzCore/CAMediaTiming.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "doomdef.h"
#include "doomstat.h"
#include "r_main.h"
#include "r_etch.h"
#include "st_stuff.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "m_argv.h"
#include "metal_backend.h"

static NSWindow *window;
static CAMetalLayer *layer;
static id<MTLDevice> device;
static id<MTLCommandQueue> queue;
static id<MTLRenderPipelineState> pipeline;
static id<MTLTexture> indices;
static byte colors[256 * 4];
static BOOL held[256];
static unsigned presentedFrames, runFrames;
static BOOL fileMode, fileClosed;
static BOOL gpuEtch, capturePending;
static BOOL playOnEtch,physicalRig;
static EtchPlayView *playView;
static MachineLive *live;
static double livePrevious;
static unsigned sceneGeneration,captureGeneration;
static void FinishRig(void);
static BOOL SceneActive(void)
{ return gamestate == GS_LEVEL && !menuactive && !automapactive; }
static BOOL GPUActive(void)
{ return gpuEtch && gamestate == GS_LEVEL && !menuactive && !automapactive; }
unsigned I_MetalPresentedFrames(void) { return presentedFrames; }

static void PostKey(int key, BOOL down)
{
    if (key <= 0 || key > 255) return;
    held[key] = down;
    event_t event = {down ? ev_keydown : ev_keyup, key, 0, 0};
    D_PostEvent(&event);
}
@interface DoomWindowDelegate : NSObject <NSWindowDelegate>
@end
@implementation DoomWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender
{
    (void)sender;
    if (fileMode) fileClosed = YES;
    else I_Quit();
    return NO;
}
- (void)windowDidResignKey:(NSNotification *)note
{
    (void)note;
    for (int key = 1; key < 256; key++)
        if (held[key]) PostKey(key, NO);
}
@end
static DoomWindowDelegate *windowDelegate;

static void ReadFrameLimit(void)
{
    int option = M_CheckParm("--run-frames");
    if (!option) return;
    if (option + 1 >= myargc) I_Error("--run-frames requires a positive integer");
    const char *text = myargv[option + 1];
    if (!*text || strspn(text, "0123456789") != strlen(text))
        I_Error("--run-frames requires a positive integer");
    errno = 0;
    unsigned long value = strtoul(text, NULL, 10);
    if (errno == ERANGE || !value || value > UINT_MAX)
        I_Error("--run-frames is outside 1..%u", UINT_MAX);
    runFrames = (unsigned)value;
}

static double ReadGPURate(void)
{
    int option = M_CheckParm("--etch-gpu-rate");
    if (!option) return 1;
    if (option + 1 >= myargc) I_Error("--etch-gpu-rate requires a number in 1/64..1024");
    const char *text = myargv[option + 1];
    char *end = NULL;
    errno = 0;
    double value = strtod(text, &end);
    if (!*text || end == text || *end || errno == ERANGE || !isfinite(value)
        || value < 1.0 / 64 || value > 1024)
        I_Error("--etch-gpu-rate must be a finite number in 1/64..1024");
    return value;
}

void I_StartFrame(void)
{
    if (physicalRig) {
        if (SceneActive() && gametic >= 20 && !capturePending && MachineLiveNeedsPath(live)) {
            if (!R_EtchBegin(320,168)) I_Error("Cannot capture the physical rig scene");
            capturePending=YES; captureGeneration=sceneGeneration;
        }
        return;
    }
    if (!gpuEtch) return;
    BOOL active = GPUActive();
    EtchGPUTick(CACurrentMediaTime(), active);
    if (active && gametic >= 20 && !capturePending && EtchGPUNeedsPath()) {
        if (!R_EtchBegin(320, 168)) I_Error("Cannot start GPU scene capture");
        capturePending = YES;
    }
}
static void ToggleGPULayer(int key)
{
    switch (key) {
    case KEY_F2: R_EtchHatchPattern((R_EtchHatchPatternValue() + 1) % 3); break;
    case KEY_F3: R_EtchSpriteTextures(!R_EtchSpriteTexturesEnabled()); break;
    case KEY_F4: R_EtchTextures(!R_EtchTexturesEnabled()); break;
    case KEY_F5: R_EtchSprites(!R_EtchSpritesEnabled()); break;
    case 'h':
        R_EtchHUD(!R_EtchHUDEnabled());
        R_SetViewSize(R_EtchHUDEnabled() ? 10 : 11, 0);
        break;
    case 'g': R_EtchWeapon(!R_EtchWeaponEnabled()); break;
    case 'j': R_EtchWeaponTextures(!R_EtchWeaponTexturesEnabled()); break;
    }
    if (physicalRig) { sceneGeneration++; MachineLiveErase(live); }
    else EtchGPUInvalidate();
    [playView invalidate];
}

static BOOL HandleGPUKey(int key, BOOL pressed)
{
    if (!gpuEtch && !physicalRig) return NO;
    BOOL layerKey = key == KEY_F2 || key == KEY_F3 || key == KEY_F4 || key == KEY_F5
        || key == 'h' || key == 'g' || key == 'j';
    BOOL playbackKey = key == KEY_F7 || key == KEY_F8 || key == KEY_F9
        || key == '+' || key == '=' || key == '-' || key == '_';
    if (!layerKey && !playbackKey) return NO;
    if (pressed) {
        if (key==KEY_F7 || key==KEY_F8) [playView invalidate];
        if (layerKey) ToggleGPULayer(key);
        else if (physicalRig) {
            if (key==KEY_F7) { sceneGeneration++; MachineLiveErase(live); }
            if (key==KEY_F9) {
                MachineLivePause(live,!MachineLivePaused(live));
                livePrevious=CACurrentMediaTime();
            }
        } else EtchGPUAction(key);
    }
    return YES;
}

void I_StartTic(void)
{
    if (!window) return;
    @autoreleasepool {
        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES])) {
            BOOL keyEvent = event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp;
            if (event.window == window && keyEvent) {
                NSEventModifierFlags chord=NSEventModifierFlagControl | NSEventModifierFlagCommand;
                if (playOnEtch && (event.modifierFlags & chord)==chord &&
                    [event.charactersIgnoringModifiers.lowercaseString isEqualToString:@"f"]) {
                    if (held[KEY_RCTRL]) PostKey(KEY_RCTRL,NO);
                    if (held['f']) PostKey('f',NO);
                    if (event.type==NSEventTypeKeyDown && !event.isARepeat) [window toggleFullScreen:nil];
                    continue;
                }
                if (event.modifierFlags & NSEventModifierFlagCommand) {
                    if (event.type == NSEventTypeKeyDown &&
                        [event.charactersIgnoringModifiers.lowercaseString isEqualToString:@"q"])
                        I_Quit();
                    DoomDispatchKeyEvent(event,held,^(int key,BOOL down) { PostKey(key,down); });
                    [NSApp sendEvent:event];
                    continue;
                }
                int key = DoomKeyForEvent(event);
                if (HandleGPUKey(key, event.type == NSEventTypeKeyDown && !event.isARepeat))
                    continue;
                DoomDispatchKeyEvent(event,held,^(int code,BOOL down) { PostKey(code,down); });
                continue;
            }
            if (event.window == window && event.type == NSEventTypeFlagsChanged) {
                int key = DoomKeyForEvent(event);
                NSEventModifierFlags mask = 0;
                if (key == KEY_RSHIFT) mask = NSEventModifierFlagShift;
                if (key == KEY_RCTRL) mask = NSEventModifierFlagControl;
                if (key == KEY_RALT) mask = NSEventModifierFlagOption;
                if (mask) PostKey(key, (event.modifierFlags & mask) != 0);
                continue;
            }
            [NSApp sendEvent:event];
        }
        [NSApp updateWindows];
    }
}

// A full-screen triangle samples the original indexed image through its palette.
static NSString *const shaderSource =
    @"#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct Vertex { float4 position [[position]]; float2 uv; };\n"
    "vertex Vertex vertexMain(uint i [[vertex_id]]) {\n"
    "  float2 p[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};\n"
    "  Vertex out; out.position = float4(p[i],0,1);\n"
    "  out.uv = float2((p[i].x+1)*0.5,(1-p[i].y)*0.5); return out;\n"
    "}\n"
    "fragment float4 fragmentMain(Vertex in [[stage_in]],\n"
    "    texture2d<uint> frame [[texture(0)]], constant uchar4 *pal [[buffer(0)]]) {\n"
    "  uint2 xy = uint2(clamp(in.uv,float2(0),float2(0.999999))*float2(320,200));\n"
    "  return float4(pal[frame.read(xy).r])/255.0;\n"
    "}\n";

static void InitRenderer(void)
{
    device = MTLCreateSystemDefaultDevice();
    if (!device) I_Error("Metal is unavailable on this Mac");
    queue = [device newCommandQueue];
    if (!queue) I_Error("Cannot create Metal command queue");
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) I_Error("Metal shader: %s", error.localizedDescription.UTF8String);
    MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
    desc.vertexFunction = [library newFunctionWithName:@"vertexMain"];
    desc.fragmentFunction = [library newFunctionWithName:@"fragmentMain"];
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (!pipeline) I_Error("Metal pipeline: %s", error.localizedDescription.UTF8String);
    MTLTextureDescriptor *texture = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint width:320 height:200 mipmapped:NO];
    texture.usage = MTLTextureUsageShaderRead;
    indices = [device newTextureWithDescriptor:texture];
    if (!indices) I_Error("Cannot create indexed framebuffer texture");
}

void I_InitGraphics(void)
{
    presentedFrames = runFrames = 0;
    ReadFrameLimit();
    playOnEtch=M_CheckParm("--play-on-etch")!=0;
    physicalRig=playOnEtch && M_CheckParm("--etch-via-gcode");
    gpuEtch = (playOnEtch && !physicalRig) || M_CheckParm("--etch-gpu") != 0;
    double gpuRate = ReadGPURate();
    BOOL gpuInstant = (playOnEtch && !physicalRig) || M_CheckParm("--etch-gpu-instant") != 0;
    if (!gpuEtch && (M_CheckParm("--etch-gpu-rate") || gpuInstant))
        I_Error("--etch-gpu-rate and --etch-gpu-instant require --etch-gpu");
    capturePending = NO;
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        InitRenderer();
        if (gpuEtch) {
            // Visual playback speed, independent of the later physical simulator.
            EtchGPUInit(device, 1200);
            EtchGPUSetRate(gpuRate);
            EtchGPUSetInstant(gpuInstant);
            if (EtchGPUError()) I_Error("Etch GPU: %s", EtchGPUError());
            R_SetViewSize(10, 0);
        }
        if (physicalRig) {
            live=MachineLiveCreate(NULL,(MotionPoint){0,0});
            if (!live) I_Error("Cannot create the physical live rig");
            R_SetViewSize(10,0);
        }
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 960, 720)
            styleMask:style backing:NSBackingStoreBuffered defer:NO];
        window.title = @"doom-etch";
        window.contentMinSize = NSMakeSize(320, 240);
        window.releasedWhenClosed = NO;
        windowDelegate = [DoomWindowDelegate new];
        window.delegate = windowDelegate;
        if (playOnEtch) {
            playView=[[EtchPlayView alloc] initWithFrame:window.contentView.bounds limit:runFrames];
            window.contentView=playView;
            window.collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
        } else {
            layer = [CAMetalLayer layer];
            layer.device = device;
            layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            layer.framebufferOnly = YES;
            window.contentView.wantsLayer = YES;
            window.contentView.layer = layer;
        }
        [window center];
        [window makeKeyAndOrderFront:nil];
        [NSApp finishLaunching];
        [NSApp activateIgnoringOtherApps:YES];
        if (playView) {
            [playView waitForModel];
            if (M_CheckParm("--fullscreen")) [window toggleFullScreen:nil];
        }
        livePrevious=CACurrentMediaTime();
    }
}

void I_ShutdownGraphics(void)
{
    [playView stop]; playView=nil;
    MachineLiveDestroy(live); live=NULL;
    if (gpuEtch) {
        EtchGPUShutdown();
        fprintf(stderr, "Etch GPU: frames=%u uploads=%u\n",
            EtchGPUFrameCount(), EtchGPUUploadCount());
    }
    if (capturePending) {
        EtchSnapshot snapshot = {0};
        R_EtchTakeSnapshot(&snapshot);
        free(snapshot.segments);
        capturePending = NO;
    }
    for (int key = 1; key < 256; key++)
        if (held[key]) PostKey(key, NO);
    [window orderOut:nil];
    window.delegate = nil;
    [window close];
    window = nil;
    windowDelegate = nil;
    layer = nil;
    indices = nil;
    pipeline = nil;
    queue = nil;
    device = nil;
}
void I_SetPalette(byte *palette)
{
    for (int i = 0; i < 256; i++) {
        for (int channel = 0; channel < 3; channel++)
            colors[i * 4 + channel] = gammatable[usegamma][palette[i * 3 + channel]];
        colors[i * 4 + 3] = 255;
    }
}
void I_UpdateNoBlit(void) {}
void I_WaitVBL(int count)
{
    // Sleep one retrace at a time to avoid overflowing the microsecond count.
    while (count-- > 0) usleep(1000000 / 70);
}
void I_ReadScreen(byte *dest) { memcpy(dest, screens[0], SCREENWIDTH * SCREENHEIGHT); }
void I_BeginRead(void) {}
void I_EndRead(void) {}

static void EncodeFrame(id<MTLRenderCommandEncoder> encoder, NSSize size)
{
    // Correct Doom's non-square pixels and letterbox resizable windows at 4:3.
    double width = size.width, height = width * 0.75;
    if (height > size.height) {
        height = size.height;
        width = height * 4.0 / 3.0;
    }
    double x = (size.width - width) / 2, y = (size.height - height) / 2;
    [encoder setViewport:(MTLViewport){x, y, width, height, 0, 1}];
    [encoder setRenderPipelineState:pipeline];
    [encoder setFragmentTexture:indices atIndex:0];
    [encoder setFragmentBytes:colors length:sizeof(colors) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];
}

static void PublishCapture(BOOL active)
{
    if (!capturePending) return;
    if (active) ST_EtchDrawer();
    EtchSnapshot snapshot = {0};
    int captured = R_EtchTakeSnapshot(&snapshot);
    capturePending = NO;
    if (active && !captured) I_Error("Cannot snapshot the GPU scene");
    if (active && captured) EtchGPUOfferSnapshot(&snapshot);
    free(snapshot.segments);
}

static NSString *LayerState(int enabled) { return enabled ? @"on" : @"off"; }
static void UpdateGPUTitle(void)
{
    static NSString *patterns[] = {@"horizontal", @"diagonal", @"cross"};
    window.title = [NSString stringWithFormat:
        @"doom-etch · %@ · F2 %@ · F3 %@ · F4 %@ · F5 %@ · H %@ · G %@ · J %@",
        EtchGPUStatus(), patterns[R_EtchHatchPatternValue()],
        LayerState(R_EtchSpriteTexturesEnabled()), LayerState(R_EtchTexturesEnabled()),
        LayerState(R_EtchSpritesEnabled()), LayerState(R_EtchHUDEnabled()),
        LayerState(R_EtchWeaponEnabled()), LayerState(R_EtchWeaponTexturesEnabled())];
}

static BOOL DrawGPU(void)
{
    BOOL active = GPUActive();
    EtchGPUTick(CACurrentMediaTime(), active); // Recheck after the engine tic wait.
    PublishCapture(active);
    UpdateGPUTitle();
    const char *error = EtchGPUError();
    if (error) I_Error("Etch GPU: %s", error);
    if (!active) return NO;
    presentedFrames = EtchGPUFrameCount();
    if (runFrames && presentedFrames >= runFrames) I_Quit();
    EtchGPUDraw(queue, layer, window.backingScaleFactor, runFrames);
    return YES;
}

static void FinishUpdate(const byte *frame)
{
    if (!window || !window.isVisible || window.isMiniaturized) return;
    @autoreleasepool {
        NSSize size = [window.contentView convertRectToBacking:window.contentView.bounds].size;
        if (size.width < 1 || size.height < 1) return;
        layer.drawableSize = size;
        if (gpuEtch && DrawGPU()) return;
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if (!drawable) return;
        [indices replaceRegion:MTLRegionMake2D(0, 0, 320, 200) mipmapLevel:0
            withBytes:frame bytesPerRow:320];
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        id<MTLCommandBuffer> command = [queue commandBuffer];
        if (!command) I_Error("Cannot create Metal command buffer");
        id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
        if (!encoder) I_Error("Cannot create Metal render encoder");
        EncodeFrame(encoder, size);
        [command presentDrawable:drawable];
        [command commit];
        // The next frame may overwrite this texture only after the GPU finishes.
        [command waitUntilCompleted];
        if (command.status != MTLCommandBufferStatusCompleted)
            I_Error("Metal rendering failed: %s", command.error.localizedDescription.UTF8String);
        presentedFrames++;
        if (!fileMode && runFrames && presentedFrames == runFrames) I_Quit();
    }
}

void I_FinishUpdate(void)
{
    if (playView) { @autoreleasepool { FinishRig(); } }
    else FinishUpdate(screens[0]);
}

NSWindow *I_MetalFileOpen(void)
{
    fileMode = YES; fileClosed = NO;
    I_InitGraphics();
    return window;
}
BOOL I_MetalFileClosed(void) { return fileClosed; }
void I_MetalFileDraw(const unsigned char *ink, double fade)
{
    int shades[] = {235, (int)(35 + 200 * fade), (int)(180 + 55 * fade)};
    for (unsigned i = 0; i < 256; i++) {
        for (unsigned c = 0; c < 3; c++) colors[i * 4 + c] = (byte)shades[i < 3 ? i : 0];
        colors[i * 4 + 3] = 255;
    }
    FinishUpdate(ink);
}

static void PublishPhysicalCapture(BOOL active)
{
    if (!capturePending) return;
    if (active) ST_EtchDrawer();
    EtchSnapshot snapshot={0};
    int captured=R_EtchTakeSnapshot(&snapshot);
    capturePending=NO;
    if (!active || captureGeneration!=sceneGeneration || !MachineLiveNeedsPath(live)) {
        free(snapshot.segments); return;
    }
    if (!captured) I_Error("Cannot snapshot the physical rig scene");
    MotionPoint cursor=EtchDeviceGetStatus(MachineLiveDevice(live)).cursor_mm;
    MotionPoint start={cursor.x/MOTION_WIDTH_MM*snapshot.width,cursor.y/MOTION_HEIGHT_MM*snapshot.height};
    MotionPath path={0}; MachineParseError error={0};
    int ok=MotionPlanSegments(&path,snapshot.segments,snapshot.count,snapshot.width,snapshot.height,start);
    free(snapshot.segments);
    if (ok) ok=MachineLiveSubmit(live,&path,&error);
    MotionFree(&path);
    if (!ok) I_Error("Cannot submit the physical rig scene: %s",error.message);
}
static void DrawPhysicalRig(BOOL active,double seconds)
{
    if (!MachineLiveTick(live,seconds,active)) I_Error("Cannot advance the physical rig clock");
    PublishPhysicalCapture(active);
    if (active) {
        byte palette[1024]; int shades[]={235,35,180};
        for (unsigned i=0;i<256;i++) {
            for (unsigned c=0;c<3;c++) palette[i*4+c]=(byte)shades[i<3 ? i : 0];
            palette[i*4+3]=255;
        }
        EtchStatus status=EtchDeviceGetStatus(MachineLiveDevice(live));
        NSDictionary *metadata=@{@"kind":@"indexed",@"mode":@"physical",@"width":@320,@"height":@200,
            @"cursor_mm":@[@(status.cursor_mm.x),@(status.cursor_mm.y)],
            @"motor_mm":@[@(status.motor_mm.x),@(status.motor_mm.y)],
            @"travel_mm":@[@(MOTION_WIDTH_MM),@(MOTION_HEIGHT_MM)],
            @"knob_mm_turn":@(MACHINE_ASSUMED_KNOB_MM_TURN),@"physical_seconds":@(MachineLiveSeconds(live))};
        [playView publishIndices:EtchDeviceInk(MachineLiveDevice(live)) palette:palette metadata:metadata];
    }
    window.title=[NSString stringWithFormat:@"doom-etch · physical 1× · %.3f s · %@ · F7 erase · F9 pause",
        MachineLiveSeconds(live),MachineLivePaused(live) ? @"paused" : @"unscaled motors"];
}
static void FinishRig(void)
{
    double clock=CACurrentMediaTime();
    BOOL visible=window.isVisible && !window.isMiniaturized &&
        (window.occlusionState & NSWindowOcclusionStateVisible)!=0;
    [playView updateVisibility:visible now:clock];
    if (playView.failure) I_Error("Live rig: %s",playView.failure.UTF8String);
    if (!visible) { livePrevious=clock; return; }
    [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate date]];
    if (playView.failure) I_Error("Live rig: %s",playView.failure.UTF8String);
    presentedFrames=playView.presented;
    if (runFrames && presentedFrames>=runFrames) I_Quit();
    double now=CACurrentMediaTime(),seconds=fmax(0,now-livePrevious);
    livePrevious=now; BOOL active=SceneActive();
    if (physicalRig) DrawPhysicalRig(active,seconds);
    else {
        EtchGPUTick(now,active); PublishCapture(active); UpdateGPUTitle();
        const char *error=EtchGPUError();
        if (error) I_Error("Live rig GPU: %s",error);
        if (active) [playView drawEtchWithQueue:queue];
    }
    if (!active) [playView publishIndices:screens[0] palette:colors metadata:
        @{@"kind":@"indexed",@"mode":@"game",@"width":@320,@"height":@200}];
}
