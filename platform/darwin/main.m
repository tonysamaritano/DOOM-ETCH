#import <AppKit/AppKit.h>

#include "d_main.h"
#include "m_argv.h"

int main(int argc, char **argv)
{
    @autoreleasepool {
        myargc = argc;
        myargv = argv;
        D_DoomMain();
    }
    return 0;
}
