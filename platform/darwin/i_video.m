// SPDX-License-Identifier: GPL-2.0-or-later
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import "keyboard.h"
#import "etch_gpu.h"
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
static BOOL gpuEtch, capturePending;
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
    I_Quit();
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
    EtchGPUInvalidate();
}

static BOOL HandleGPUKey(int key, BOOL pressed)
{
    if (!gpuEtch) return NO;
    BOOL layerKey = key == KEY_F2 || key == KEY_F3 || key == KEY_F4 || key == KEY_F5
        || key == 'h' || key == 'g' || key == 'j';
    BOOL playbackKey = key == KEY_F7 || key == KEY_F8 || key == KEY_F9
        || key == '+' || key == '=' || key == '-' || key == '_';
    if (!layerKey && !playbackKey) return NO;
    if (pressed) {
        if (layerKey) ToggleGPULayer(key);
        else EtchGPUAction(key);
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
                if (event.modifierFlags & NSEventModifierFlagCommand) {
                    if (event.type == NSEventTypeKeyDown &&
                        [event.charactersIgnoringModifiers.lowercaseString isEqualToString:@"q"])
                        I_Quit();
                    [NSApp sendEvent:event];
                    continue;
                }
                int key = DoomKeyForEvent(event);
                if (HandleGPUKey(key, event.type == NSEventTypeKeyDown && !event.isARepeat))
                    continue;
                if (!event.isARepeat)
                    PostKey(key, event.type == NSEventTypeKeyDown);
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
    gpuEtch = M_CheckParm("--etch-gpu") != 0;
    double gpuRate = ReadGPURate();
    BOOL gpuInstant = M_CheckParm("--etch-gpu-instant") != 0;
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
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 960, 720)
            styleMask:style backing:NSBackingStoreBuffered defer:NO];
        window.title = @"doom-etch";
        window.contentMinSize = NSMakeSize(320, 240);
        window.releasedWhenClosed = NO;
        windowDelegate = [DoomWindowDelegate new];
        window.delegate = windowDelegate;
        layer = [CAMetalLayer layer];
        layer.device = device;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly = YES;
        window.contentView.wantsLayer = YES;
        window.contentView.layer = layer;
        [window center];
        [window makeKeyAndOrderFront:nil];
        [NSApp finishLaunching];
        [NSApp activateIgnoringOtherApps:YES];
    }
}

void I_ShutdownGraphics(void)
{
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

static BOOL DrawGPU(void)
{
    BOOL active = GPUActive();
    PublishCapture(active);
    const char *error = EtchGPUError();
    if (error) I_Error("Etch GPU: %s", error);
    if (!active) return NO;
    presentedFrames = EtchGPUFrameCount();
    if (runFrames && presentedFrames >= runFrames) I_Quit();
    EtchGPUDraw(queue, layer, window.backingScaleFactor, runFrames);
    return YES;
}

void I_FinishUpdate(void)
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
            withBytes:screens[0] bytesPerRow:320];
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
        if (runFrames && presentedFrames == runFrames) I_Quit();
    }
}
