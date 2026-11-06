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
