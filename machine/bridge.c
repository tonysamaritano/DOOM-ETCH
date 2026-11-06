// SPDX-License-Identifier: GPL-2.0-or-later
#include "bridge.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static int fail(MachineParseError *e,size_t line,const char *message)
{
    if (e) { e->line=line; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
static char *read_text(FILE *stream,MachineParseError *e)
{
    char *text=NULL; size_t count=0,capacity=0,line=1; int c;
    while ((c=fgetc(stream))!=EOF) {
        if (!c) { fail(e,line,"NUL byte in stream"); free(text); return NULL; }
        if (count+1>=capacity) {
            size_t next=capacity ? capacity*2 : 4096;
            if (next<=capacity) { fail(e,line,"input too large"); free(text); return NULL; }
            char *allocation=realloc(text,next);
            if (!allocation) { fail(e,line,"out of memory"); free(text); return NULL; }
            text=allocation; capacity=next;
        }
        text[count++]=(char)c; if (c=='\n') line++;
    }
    if (ferror(stream)) { fail(e,line,"stream read error"); free(text); return NULL; }
    if (!text) text=calloc(1,1);
    if (!text) { fail(e,0,"out of memory"); return NULL; }
    text[count]=0; return text;
}
static int executable(const char *p)
{
    while (*p) {
        if (*p==' ' || *p=='\t') { p++; continue; }
        if (*p=='(') { p=strchr(p+1,')'); if (!p) return 1; p++; continue; }
        return *p!=';';
    }
    return 0;
}
static int metadata(const char *text,MachineSettings *s,MachineParseOptions *start,
                    MachineParseError *e)
{
    int has_size=0,has_start=0,commands=0; size_t line=1;
    for (const char *p=text;*p;line++) {
        const char *end=strchr(p,'\n'); size_t n=end ? (size_t)(end-p) : strlen(p);
        if (n && p[n-1]=='\r') n--;
        if (n>80) return fail(e,line,"line exceeds 80 bytes");
        char row[81]; memcpy(row,p,n); row[n]=0;
        char *q=row; while (*q==' ' || *q=='\t') q++;
        int size=!strncmp(q,"; ETCH_SIZE",11),position=!strncmp(q,"; ETCH_START",12);
        if ((size && (q[11]==' ' || !q[11])) || (position && (q[12]==' ' || !q[12]))) {
            if (commands) return fail(e,line,"metadata must precede commands");
            if (size ? has_size++ : has_start++) return fail(e,line,"duplicate machine metadata");
            const char *values=q+(size ? 11 : 12); double x,y; int used=0;
            if (sscanf(values," X%lf Y%lf %n",&x,&y,&used)!=2 || !used || values[used] ||
                !isfinite(x) || !isfinite(y)) return fail(e,line,"invalid machine metadata");
            if (size) {
                s->width_mm=x; s->height_mm=y;
                if (!has_start) start->start_cnc_mm.y=y;
                if (!MachineSettingsValidate(s,e)) { if (e) e->line=line; return 0; }
            } else start->start_cnc_mm=(MotionPoint){x,y};
        }
        if (executable(q)) commands=1;
        if (!end) break;
        p=end+1;
    }
    return 1;
}
int MachineRead(FILE *stream,MachineSettings *settings,MachineProgram *program,MachineParseError *e)
{
    if (!stream || !settings || !program) return fail(e,0,"missing controller input");
    if (!MachineSettingsValidate(settings,e)) return 0;
    char *text=read_text(stream,e); if (!text) return 0;
    MachineSettings next=*settings;
    MachineParseOptions start={{0,next.height_mm}};
    int ok=metadata(text,&next,&start,e);
    if (ok && (start.start_cnc_mm.x<0 || start.start_cnc_mm.y<0 ||
        start.start_cnc_mm.x>next.width_mm || start.start_cnc_mm.y>next.height_mm))
        ok=fail(e,0,"required start outside travel");
    if (ok) ok=MachineGCodeParse(program,text,&start,e);
    free(text); if (ok) *settings=next;
    return ok;
}
int MachineSubmitMotion(EtchDevice *device,const MotionPath *path,double feed,MachineParseError *e)
{
    if (!device || !MotionValid(path) || !isfinite(feed) || feed<=0 || feed/60<=0 ||
        path->count>SIZE_MAX/sizeof(MotionPoint)) return fail(e,0,"invalid direct motion");
    MotionPoint *points=malloc(path->count*sizeof(*points));
    if (!points) return fail(e,0,"out of memory");
    for (size_t i=0;i<path->count;i++)
        points[i]=(MotionPoint){path->points[i].x/path->width*path->width_mm,
                               path->points[i].y/path->height*path->height_mm};
    int ok=EtchDeviceSubmit(device,points,path->count,feed/60);
    free(points);
    return ok ? 1 : fail(e,0,"device rejected direct motion");
}
static MotionPoint physical(const MachineStepProgram *p,const int64_t steps[2])
{
    return (MotionPoint){steps[0]/p->settings.steps_per_mm[0],
                         steps[1]/p->settings.steps_per_mm[1]};
}
int MachineSubmitBatch(EtchDevice *device,const MachineStepProgram *p,size_t first,
                       size_t count,MachineParseError *e)
{
    if (!device || !p || !count || !p->moves || first>p->count || count>p->count-first ||
        count>=SIZE_MAX/sizeof(MotionPoint) || !MachineSettingsValidate(&p->settings,e))
        return fail(e,0,"invalid uniform-feed batch");
    double feed=p->moves[first].feed_mm_s;
    MotionPoint *points=malloc((count+1)*sizeof(*points));
    if (!points) return fail(e,0,"out of memory");
    points[0]=physical(p,first ? p->moves[first-1].target_steps : p->start_steps);
    for (size_t i=0;i<count;i++) {
        const MachineStep *step=&p->moves[first+i];
        if ((step->kind!=MACHINE_MOVE_DRAW && step->kind!=MACHINE_MOVE_RAPID) ||
            step->feed_mm_s!=feed || !isfinite(feed) || feed<0 ||
            (step->kind==MACHINE_MOVE_DRAW && feed==0)) {
            free(points); return fail(e,step->line,"batch requires motion with one feed");
        }
        points[i+1]=physical(p,step->target_steps);
    }
    int ok=feed>0 ? EtchDeviceSubmit(device,points,count+1,feed) :
                    EtchDeviceSubmitHardware(device,points,count+1);
    free(points);
    return ok ? 1 : fail(e,p->moves[first].line,"device rejected controller batch");
}
