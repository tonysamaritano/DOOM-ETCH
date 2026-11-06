// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
unsigned I_MetalPresentedFrames(void);
#ifdef __OBJC__
#import <AppKit/AppKit.h>
NSWindow *I_MetalFileOpen(void);
BOOL I_MetalFileClosed(void);
void I_MetalFileDraw(const unsigned char *ink, double fade);
#endif
