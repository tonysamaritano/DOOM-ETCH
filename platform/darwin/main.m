#import <AppKit/AppKit.h>
#include "file_play.h"

#include "d_main.h"
#include "m_argv.h"

int main(int argc, char **argv)
{
    @autoreleasepool {
        myargc = argc;
        myargv = argv;
        int standalone = EtchFileMain(argc, argv);
        if (standalone >= 0) return standalone;
        D_DoomMain();
    }
    return 0;
}
