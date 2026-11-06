// SPDX-License-Identifier: GPL-2.0-or-later
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMediaTiming.h>
#import "file_play.h"
#import "metal_backend.h"
#import "keyboard.h"
#include "doomdef.h"
#include "i_video.h"
#include "run.h"
#include "rate.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct {
    const char *filename;
    const char **assignments;
    size_t count;
    double rate;
    unsigned frames;
} FileOptions;
static int rate_value(const char *text,double *rate)
{
    char *end; errno=0; double value=strtod(text,&end);
    if (!*text || end==text || *end || errno==ERANGE || !isfinite(value) ||
        value<ETCH_VISUAL_MIN_RATE || value>ETCH_VISUAL_MAX_RATE) return 0;
    *rate=value; return 1;
}
static int frame_value(const char *text,unsigned *frames)
{
    if (!*text || strspn(text,"0123456789")!=strlen(text)) return 0;
    errno=0; unsigned long value=strtoul(text,NULL,10);
    if (errno==ERANGE || !value || value>UINT_MAX) return 0;
    *frames=(unsigned)value; return 1;
}
static int options(int argc,char **argv,FileOptions *o)
{
    o->rate=1; int rate_seen=0;
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--etch-gcode")) {
            if (o->filename || i+1==argc || !strncmp(argv[i+1],"--",2)) return 0;
            o->filename=argv[++i];
        } else if (!strcmp(argv[i],"--etch-time-scale")) {
            if (rate_seen++ || i+1==argc || !rate_value(argv[++i],&o->rate)) {
                fputs("--etch-time-scale requires a finite rate in 1/64..1024\n",stderr); return 0;
            }
        } else if (!strcmp(argv[i],"--run-frames")) {
            if (o->frames || i+1==argc || !frame_value(argv[++i],&o->frames)) return 0;
        } else if (!strcmp(argv[i],"--settings")) {
            size_t before=o->count;
            while (i+1<argc && strncmp(argv[i+1],"--",2)) o->assignments[o->count++]=argv[++i];
            if (before==o->count) return 0;
        } else return 0;
    }
    return o->filename!=NULL;
}
typedef struct {
    MachinePlayback *play;
    double rate,shake_time,last;
    BOOL paused,queued,shaking,shaken,reported,reset_clock;
    unsigned char retained[ETCH_INK_WIDTH*ETCH_INK_HEIGHT];
} Presentation;
static int replay(Presentation *p)
{
    MachineParseError error={0};
    MachinePlayback *fresh=MachinePlaybackCreate(MachinePlaybackProgram(p->play),&error);
    if (!fresh) { fprintf(stderr,"Replay: %s\n",error.message); return 0; }
    MachinePlaybackDestroy(p->play); p->play=fresh;
    p->paused=p->queued=p->shaking=p->shaken=p->reported=NO;
    p->shake_time=0; p->reset_clock=YES; memset(p->retained,0,sizeof(p->retained));
    return 1;
}
static int events(Presentation *p,NSWindow *window,BOOL *quit)
{
    NSEvent *event;
    while ((event=[NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast]
        inMode:NSDefaultRunLoopMode dequeue:YES])) {
        if (event.window==window && event.type==NSEventTypeKeyDown) {
            if ((event.modifierFlags & NSEventModifierFlagCommand) &&
                [event.charactersIgnoringModifiers.lowercaseString isEqualToString:@"q"])
                *quit=YES;
            else if (!event.isARepeat) {
                int key=DoomKeyForEvent(event);
                p->rate=EtchVisualRateKey(p->rate,key);
                if (key==KEY_F6 && !replay(p)) return 0;
                if (key==KEY_F9) { p->paused=!p->paused; p->reset_clock=YES; }
                if (key==KEY_F7 && !p->shaken && !p->shaking) p->queued=YES;
            }
            continue;
        }
        [NSApp sendEvent:event];
    }
    [NSApp updateWindows];
    return 1;
}
static void retain_ghost(Presentation *p)
{
    const unsigned char *ink=EtchDeviceInk(MachinePlaybackDevice(p->play));
    const unsigned cells=ETCH_INK_WIDTH*ETCH_INK_HEIGHT;
    unsigned marks=0;
    for (unsigned i=0;i<cells;i++) marks+=ink[i]!=ETCH_INK_EMPTY;
    double fraction=MachinePlaybackProgram(p->play)->settings.ghost_fraction;
    unsigned keep=(unsigned)floor(marks*fraction);
    memset(p->retained,0,sizeof(p->retained));
    /* Stable permutation distributes the assumed retained fraction spatially. */
    for (unsigned i=0;i<cells && keep;i++) {
        unsigned at=(i*9973+7919)%cells;
        if (ink[at]) { p->retained[at]=ETCH_INK_GHOST; keep--; }
    }
    p->shaking=NO; p->shaken=YES;
}
static int advance(Presentation *p,double seconds)
{
    if (!p->paused && !MachinePlaybackDone(p->play) && !MachinePlaybackTick(p->play,seconds)) {
        const MachineParseError *e=MachinePlaybackError(p->play);
        fprintf(stderr,"Playback:%zu: %s\n",e->line,e->message); return 0;
    }
    if (MachinePlaybackDone(p->play)) {
        if (!p->reported) {
            if (!MachineReport(stdout,MachinePlaybackProgram(p->play),MachinePlaybackSeconds(p->play),1))
                return 0;
            p->reported=YES;
        }
        if (p->queued && !p->paused) {
            p->queued=NO; p->shaking=YES; p->shake_time=0;
            return 1;
        }
    }
    if (p->shaking && !p->paused) {
        p->shake_time+=seconds;
        if (p->shake_time>=MachinePlaybackProgram(p->play)->settings.erase_seconds) retain_ghost(p);
    }
    return 1;
}
static void title(Presentation *p,NSWindow *window)
{
    NSString *state=p->paused ? @"paused" : p->shaking ? @"presentation shake" :
        p->queued ? @"shake queued after file" : MachinePlaybackDone(p->play) ? @"file completed" : @"playing";
    window.title=[NSString stringWithFormat:@"doom-etch · %.3g× · %.3f physical s · %@ · F6 replay · F7 shake · F9 pause",
        p->rate,MachinePlaybackSeconds(p->play),state];
}
