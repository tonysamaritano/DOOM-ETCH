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
