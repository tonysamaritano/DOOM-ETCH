// SPDX-License-Identifier: GPL-2.0-or-later
#import "keyboard.h"
#include "doomdef.h"
#include <assert.h>
static NSEvent *key(NSEventType type,unsigned short code,NSEventModifierFlags modifiers)
{
    return [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:modifiers
        timestamp:0 windowNumber:0 context:nil characters:@"f" charactersIgnoringModifiers:@"f"
        isARepeat:NO keyCode:code];
}
int main(void)
{
    @autoreleasepool {
        BOOL held[256]={0};
        __block unsigned count=0;
        __block int last=0;
        __block BOOL down=NO;
        void (^post)(int,BOOL)=^(int code,BOOL pressed) { count++; last=code; down=pressed; };
        DoomDispatchKeyEvent(key(NSEventTypeKeyDown,126,0),held,post);
        assert(held[KEY_UPARROW] && count==1 && last==KEY_UPARROW && down);
        // Fullscreen uses Ctrl-Cmd-F; releasing movement while Cmd remains held
        // must release the actual game key, rather than leaving forward stuck.
        DoomDispatchKeyEvent(key(NSEventTypeKeyUp,126,NSEventModifierFlagCommand),held,post);
        assert(!held[KEY_UPARROW] && count==2 && last==KEY_UPARROW && !down);
        DoomDispatchKeyEvent(key(NSEventTypeKeyDown,3,NSEventModifierFlagCommand),held,post);
        assert(!held['f'] && count==2);
        DoomDispatchKeyEvent(key(NSEventTypeKeyUp,3,NSEventModifierFlagCommand),held,post);
        assert(!held['f'] && count==2);
    }
    return 0;
}
