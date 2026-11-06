// SPDX-License-Identifier: GPL-2.0-or-later
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import "keyboard.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#undef true
#undef false
#include "doomdef.h"
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

void I_StartFrame(void) {}
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
                if (!event.isARepeat)
                    PostKey(DoomKeyForEvent(event), event.type == NSEventTypeKeyDown);
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
