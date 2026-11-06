// SPDX-License-Identifier: GPL-2.0-or-later
#import "keyboard.h"
#undef true
#undef false
#include "doomdef.h"

int DoomKeyForEvent(NSEvent *event)
{
    switch (event.keyCode) {
        case 123: return KEY_LEFTARROW; case 124: return KEY_RIGHTARROW;
        case 125: return KEY_DOWNARROW; case 126: return KEY_UPARROW;
        case 53: return KEY_ESCAPE; case 36: case 76: return KEY_ENTER;
        case 48: return KEY_TAB; case 51: case 117: return KEY_BACKSPACE;
        case 122: return KEY_F1; case 120: return KEY_F2; case 99: return KEY_F3;
        case 118: return KEY_F4; case 96: return KEY_F5; case 97: return KEY_F6;
        case 98: return KEY_F7; case 100: return KEY_F8; case 101: return KEY_F9;
        case 109: return KEY_F10; case 103: return KEY_F11; case 111: return KEY_F12;
        case 56: case 60: return KEY_RSHIFT;
        case 59: case 62: return KEY_RCTRL;
        case 58: case 61: return KEY_RALT;
        case 113: return KEY_PAUSE;
    }
    if (event.type != NSEventTypeKeyDown && event.type != NSEventTypeKeyUp)
        return 0;
    NSString *text = event.charactersIgnoringModifiers.lowercaseString;
    if (!text.length) return 0;
    unichar key = [text characterAtIndex:0];
    return key >= 32 && key < 127 ? key : 0;
}
