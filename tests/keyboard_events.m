#import "keyboard.h"
#include "doomdef.h"
#include <assert.h>

static NSEvent *key(NSEventType type, unsigned short code, NSString *characters)
{
    return [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:0
                          timestamp:0 windowNumber:0 context:nil
                         characters:characters charactersIgnoringModifiers:characters
                          isARepeat:NO keyCode:code];
}

int main(void)
{
    @autoreleasepool {
        assert(DoomKeyForEvent(key(NSEventTypeKeyDown, 0, @"A")) == 'a');
        assert(DoomKeyForEvent(key(NSEventTypeKeyUp, 123, @"")) == KEY_LEFTARROW);
        assert(DoomKeyForEvent(key(NSEventTypeFlagsChanged, 56, @"")) == KEY_RSHIFT);
        assert(DoomKeyForEvent(key(NSEventTypeFlagsChanged, 59, @"")) == KEY_RCTRL);
        assert(DoomKeyForEvent(key(NSEventTypeFlagsChanged, 58, @"")) == KEY_RALT);
        // Caps Lock and Command change flags without delivering characters.
        assert(DoomKeyForEvent(key(NSEventTypeFlagsChanged, 57, @"")) == 0);
        assert(DoomKeyForEvent(key(NSEventTypeFlagsChanged, 55, @"")) == 0);
    }
    return 0;
}
