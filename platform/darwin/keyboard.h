// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#import <AppKit/AppKit.h>
int DoomKeyForEvent(NSEvent *event);
/* Dispatch game key-down/up events; command shortcuts stay with Cocoa. */
void DoomDispatchKeyEvent(NSEvent *event,BOOL held[256],void (^post)(int key,BOOL down));
