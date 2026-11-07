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
            if (o->filename) { fputs("Duplicate --etch-gcode option\n",stderr); return 0; }
            if (i+1==argc || !strncmp(argv[i+1],"--",2)) {
                fputs("--etch-gcode requires FILE\n",stderr); return 0;
            }
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
    EtchTrace *trace; /* Owned; the playback borrows it for drawing strokes. */
    MotionTimedSegment *strokes;
    size_t stroke_capacity;
    double ink_level; /* Visible fraction left by completed F7 shakes. */
    double rate,shake_time,last;
    BOOL paused,interrupted,shaking,shaken,reported,reset_clock;
    unsigned char retained[ETCH_INK_WIDTH*ETCH_INK_HEIGHT];
} Presentation;
static void free_trace(EtchTrace *trace)
{
    if (trace) { EtchTraceFree(trace); free(trace); }
}
static int replay(Presentation *p)
{
    MachineParseError error={0};
    EtchTrace *trace=p->trace ? calloc(1,sizeof(*trace)) : NULL;
    if (p->trace && !trace) { fputs("Replay: out of memory\n",stderr); return 0; }
    MachinePlayback *fresh=MachinePlaybackCreateTraced(MachinePlaybackProgram(p->play),trace,&error);
    if (!fresh) { free_trace(trace); fprintf(stderr,"Replay: %s\n",error.message); return 0; }
    MachinePlaybackDestroy(p->play); p->play=fresh;
    free_trace(p->trace); p->trace=trace; p->ink_level=1;
    p->paused=p->interrupted=p->shaking=p->shaken=p->reported=NO;
    p->shake_time=0; p->reset_clock=YES; memset(p->retained,0,sizeof(p->retained));
    return 1;
}
static int control(Presentation *p,int key)
{
    p->rate=EtchVisualRateKey(p->rate,key);
    if (key==KEY_F6 && !replay(p)) return 0;
    if (key==KEY_F9) { p->paused=!p->paused; p->reset_clock=YES; }
    if (key==KEY_F7 && !p->shaking) {
        if (!p->interrupted)
            memcpy(p->retained,EtchDeviceInk(MachinePlaybackDevice(p->play)),sizeof(p->retained));
        p->interrupted=YES; p->shaking=YES; p->shaken=NO;
        p->shake_time=0; p->reset_clock=YES;
    }
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
                if (!control(p,key)) return 0;
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
    const unsigned char *ink=p->retained;
    const unsigned cells=ETCH_INK_WIDTH*ETCH_INK_HEIGHT;
    unsigned marks=0;
    for (unsigned i=0;i<cells;i++) marks+=ink[i]!=ETCH_INK_EMPTY;
    double fraction=MachinePlaybackProgram(p->play)->settings.ghost_fraction;
    unsigned keep=(unsigned)floor(marks*fraction);
    /* Stable permutation distributes the assumed retained fraction spatially. */
    for (unsigned i=0;i<cells;i++) {
        unsigned at=(i*9973+7919)%cells;
        if (ink[at]) {
            p->retained[at]=keep ? ETCH_INK_GHOST : ETCH_INK_EMPTY;
            if (keep) keep--;
        }
    }
    p->shaking=NO; p->shaken=YES;
    p->ink_level*=fraction;
}
static int advance(Presentation *p,double seconds)
{
    if (!p->paused && !p->interrupted && !MachinePlaybackDone(p->play) && !MachinePlaybackTick(p->play,seconds)) {
        const MachineParseError *e=MachinePlaybackError(p->play);
        fprintf(stderr,"Playback:%zu: %s\n",e->line,e->message); return 0;
    }
    if (MachinePlaybackDone(p->play) && !p->interrupted) {
        if (!p->reported) {
            if (!MachineReport(stdout,MachinePlaybackProgram(p->play),MachinePlaybackSeconds(p->play),1))
                return 0;
            p->reported=YES;
        }
    }
    if (p->shaking && !p->paused) {
        p->shake_time+=seconds;
        if (p->shake_time>=MachinePlaybackProgram(p->play)->settings.erase_seconds) retain_ghost(p);
    }
    return 1;
}
/* The newest file erase (M0) that has started by this clock, or NULL. */
static const EtchTraceErase *last_erase(const EtchTrace *t,double clock)
{
    const EtchTraceErase *found=NULL;
    for (size_t i=0;i<t->erase_count && t->erases[i].t0<=clock;i++) found=&t->erases[i];
    return found;
}
/* Copy traced stylus segments into GPU strokes: the frame before the newest
 * erase becomes the ghost layer, later segments the current layer. */
static int build_strokes(Presentation *p,EtchGPUStrokes *ghost,EtchGPUStrokes *current)
{
    *ghost=*current=(EtchGPUStrokes){0};
    const EtchTrace *t=p->trace;
    if (!t || !t->count) return 1;
    if (p->stroke_capacity<t->count) {
        MotionTimedSegment *grown=realloc(p->strokes,t->count*sizeof(*grown));
        if (!grown) return 0;
        p->strokes=grown; p->stroke_capacity=t->count;
    }
    double clock=MachinePlaybackSeconds(p->play),w=t->config.width_mm,h=t->config.height_mm;
    const EtchTraceErase *erase=last_erase(t,clock);
    const EtchTraceErase *before=erase && erase>t->erases ? erase-1 : NULL;
    double ghost_from=before ? before->t0 : 0,current_from=erase ? erase->t0 : 0;
    size_t count=0,split=0;
    for (size_t i=0;i<t->count;i++) {
        const EtchTraceSegment *s=&t->segments[i];
        if (s->t0<ghost_from) continue;
        if (s->t0<current_from) split=count+1;
        double x1=s->x0+s->ux*s->length,y1=s->y0+s->uy*s->length;
        /* The shader reveals each stroke evenly across its traced duration. */
        p->strokes[count++]=(MotionTimedSegment){{s->x0/w,s->y0/h,x1/w,y1/h},
            {s->t0,s->t0+s->duration,0,0}};
    }
    double gf=t->config.ghost_fraction,level=p->ink_level;
    double shake=p->shaking ? fmin(1,p->shake_time/MachinePlaybackProgram(p->play)->settings.erase_seconds) : 0;
    double erasing=erase ? fmin(1,(clock-erase->t0)/fmax(erase->duration,1e-9)) : 0;
    *ghost=(EtchGPUStrokes){p->strokes,split,
        (float)(level*(1-(1-gf)*erasing)*(1-(1-gf)*shake)),(float)(1.3*fmax(erasing,shake))};
    *current=(EtchGPUStrokes){p->strokes+split,count-split,
        (float)(level*(1-(1-gf)*shake)),(float)(1.3*shake)};
    return 1;
}
static void title(Presentation *p,NSWindow *window)
{
    NSString *state=p->paused ? @"paused" : p->shaking ? @"presentation shake" :
        p->interrupted ? @"stopped" : MachinePlaybackDone(p->play) ? @"file completed" : @"playing";
    NSString *interrupted=p->interrupted ? @" · file interrupted · F6 to replay" : @"";
    char rate[32];
    EtchVisualRateText(p->rate,rate,sizeof(rate));
    NSString *text=[NSString stringWithFormat:@"doom-etch · %@ · %.3f physical s · %@%@ · F6 replay · F7 shake · F9 pause",
        @(rate),MachinePlaybackSeconds(p->play),state,interrupted];
    /* Setting an unchanged title still makes AppKit lay out the title bar. */
    if (![window.title isEqualToString:text]) window.title=text;
}
static int present(MachinePlayback *play,EtchTrace *trace,const FileOptions *o)
{
    Presentation p={.play=play,.trace=trace,.ink_level=1,.rate=o->rate,.last=CACurrentMediaTime()};
    NSWindow *window=I_MetalFileOpen();
    p.last=CACurrentMediaTime();
    BOOL quit=NO; int ok=1;
    while (!quit && !I_MetalFileClosed() && (!o->frames || I_MetalPresentedFrames()<o->frames)) {
        @autoreleasepool {
            if (!events(&p,window,&quit)) { ok=0; break; }
            double now=CACurrentMediaTime(),delta=now-p.last;
            p.last=now;
            if (p.reset_clock) { delta=0; p.reset_clock=NO; }
            if (!advance(&p,delta*p.rate)) { ok=0; break; }
            title(&p,window);
            EtchGPUStrokes ghost,current;
            if (!build_strokes(&p,&ghost,&current)) { fputs("Out of memory for strokes\n",stderr); ok=0; break; }
            I_MetalFileDraw(ghost,current,MachinePlaybackSeconds(p.play));
            usleep(1000);
        }
    }
    fprintf(stderr,"Presented %u frames\n",I_MetalPresentedFrames());
    I_ShutdownGraphics(); MachinePlaybackDestroy(p.play);
    free_trace(p.trace); free(p.strokes);
    return ok ? 0 : 1;
}
int EtchFileMain(int argc,char **argv)
{
    int file=0,scale=0;
    for (int i=1;i<argc;i++) {
        file|=!strcmp(argv[i],"--etch-gcode"); scale|=!strcmp(argv[i],"--etch-time-scale");
    }
    if (!file) {
        if (scale) { fputs("--etch-time-scale requires standalone --etch-gcode\n",stderr); return 2; }
        return -1;
    }
    FileOptions o={0}; o.assignments=calloc((size_t)argc,sizeof(*o.assignments));
    if (!o.assignments) return 1;
    if (!options(argc,argv,&o)) {
        fputs("Invalid or conflicting standalone G-code option\n",stderr);
        free(o.assignments); return 2;
    }
    MachineParseError error={0};
    EtchTrace *trace=calloc(1,sizeof(*trace));
    MachinePlayback *play=trace ? MachineLoadTraced(o.filename,o.assignments,o.count,trace,&error) : NULL;
    free(o.assignments);
    if (!play) {
        if (!trace) snprintf(error.message,sizeof(error.message),"out of memory");
        fprintf(stderr,"%s:%zu: %s\n",o.filename,error.line,error.message);
        free_trace(trace); return 1;
    }
    return present(play,trace,&o);
}
