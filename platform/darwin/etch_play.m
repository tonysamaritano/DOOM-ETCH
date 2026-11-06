// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_play.h"
#import "etch_packet.h"
#import "etch_gpu.h"
#import <WebKit/WebKit.h>
#include "i_system.h"
#include <string.h>
static BOOL argument(int argc,char **argv,const char *name)
{
    for (int i=1;i<argc;i++) if (!strcmp(argv[i],name)) return YES;
    return NO;
}
const char *EtchPlayArgumentError(int argc,char **argv)
{
    BOOL play=argument(argc,argv,"--play-on-etch");
    BOOL physical=argument(argc,argv,"--etch-via-gcode");
    if (play && argument(argc,argv,"--etch-gcode"))
        return "--play-on-etch conflicts with standalone --etch-gcode";
    if (!play && physical) return "--etch-via-gcode requires --play-on-etch";
    if (!play && argument(argc,argv,"--fullscreen")) return "--fullscreen requires --play-on-etch";
    if (play && argument(argc,argv,"--etch-time-scale"))
        return "--etch-time-scale is only valid with standalone --etch-gcode";
    if (physical && argument(argc,argv,"--etch-gpu-rate"))
        return "--etch-gpu-rate is a visual control; --etch-via-gcode is fixed physical 1x";
    if (physical && argument(argc,argv,"--etch-gpu-instant"))
        return "--etch-gpu-instant is a visual control; --etch-via-gcode is fixed physical 1x";
    if (physical && argument(argc,argv,"--etch-gpu"))
        return "--etch-gpu conflicts with physical --etch-via-gcode";
    return NULL;
}
