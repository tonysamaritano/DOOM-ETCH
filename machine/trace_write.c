// SPDX-License-Identifier: GPL-2.0-or-later
#include "trace.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int fail(MachineParseError *e,const char *message)
{
    if (e) { e->line=0; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
static int valid(const MachineStepProgram *p,const EtchTrace *t,double seconds)
{
    if (!p || !t || !t->completed || t->failed || !isfinite(seconds) || seconds<0 ||
        !isfinite(t->clock) || fabs(seconds-t->clock)>1e-9 ||
        (p->count && !p->moves) || (t->count && !t->segments) ||
        (t->erase_count && !t->erases) || !MachineSettingsValidate(&p->settings,NULL)) return 0;
    size_t segment=0,erase=0,frame=0; double end=0;
    for (size_t i=0;i<p->count;i++) {
        const MachineStep *move=&p->moves[i];
        if (move->kind<MACHINE_MOVE_PAUSE) {
            if (segment==t->count) return 0;
            const EtchTraceSegment *s=&t->segments[segment++];
            double fields[]={s->t0,s->duration,s->ramp,s->peak,s->accel,s->slack,s->length,
                s->x0,s->y0,s->ux,s->uy,s->motor_x0,s->motor_y0};
            for (unsigned j=0;j<13;j++) if (!isfinite(fields[j])) return 0;
            if (fabs(s->t0-end)>1e-9 || s->duration<0 || s->ramp<0 || s->peak<0 || s->accel<0 ||
                s->slack<0 || s->length<0 || s->line!=move->line || s->frame!=frame) return 0;
            end=s->t0+s->duration;
        } else if (move->kind==MACHINE_MOVE_PAUSE) {
            if (erase==t->erase_count) return 0;
            EtchTraceErase e=t->erases[erase++];
            if (!isfinite(e.t0) || !isfinite(e.duration) || fabs(e.t0-end)>1e-9 || e.duration<=0) return 0;
            end=e.t0+e.duration; frame++;
        }
    }
    return segment==t->count && erase==t->erase_count && isfinite(end) && fabs(end-t->clock)<=1e-9;
}
static void string(FILE *f,const char *text)
{
    fputc('"',f);
    for (const unsigned char *p=(const unsigned char *)(text ? text : "");*p;p++) {
        if (*p=='"' || *p=='\\') fprintf(f,"\\%c",*p);
        else if (*p=='\n') fputs("\\n",f);
        else if (*p=='\r') fputs("\\r",f);
        else if (*p=='\t') fputs("\\t",f);
        else if (*p<32) fprintf(f,"\\u%04x",*p);
        else fputc(*p,f);
    }
    fputc('"',f);
}
static void settings(FILE *f,const MachineSettings *s)
{
    fprintf(f,"\"settings\":{\"assumed\":true,\"width_mm\":%.17g,\"height_mm\":%.17g,"
        "\"steps_per_mm\":[%.17g,%.17g],\"max_rate_mm_min\":[%.17g,%.17g],"
        "\"acceleration_mm_s2\":[%.17g,%.17g],\"backlash_mm\":%.17g,"
        "\"erase_seconds\":%.17g,\"ghost_fraction\":%.17g,\"knob_mm_turn\":%.17g},\n",
        s->width_mm,s->height_mm,s->steps_per_mm[0],s->steps_per_mm[1],
        s->max_rate_mm_min[0],s->max_rate_mm_min[1],s->acceleration_mm_s2[0],
        s->acceleration_mm_s2[1],s->backlash_mm,s->erase_seconds,s->ghost_fraction,s->knob_mm_turn);
}
static void segments(FILE *f,const EtchTrace *t)
{
    fputs("\"segment_fields\":[\"t0\",\"duration\",\"ramp\",\"peak\",\"accel\",\"slack\",\"length\","
        "\"x0\",\"y0\",\"ux\",\"uy\",\"motor_x0\",\"motor_y0\",\"line\",\"frame\"],\n\"segments\":[",f);
    for (size_t i=0;i<t->count;i++) {
        const EtchTraceSegment *s=&t->segments[i];
        fprintf(f,"%s%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%zu,%zu",
            i ? ",\n" : "\n",s->t0,s->duration,s->ramp,s->peak,s->accel,s->slack,s->length,
            s->x0,s->y0,s->ux,s->uy,s->motor_x0,s->motor_y0,s->line,s->frame);
    }
    fputs("],\n",f);
}
static int document(FILE *f,const MachineStepProgram *p,const EtchTrace *t,double seconds,
                    const MachineRequirementsFrames *frames)
{
    fputs("{\"format\":\"doom-etch-trace\",\"version\":1,\n"
        "\"units\":{\"length\":\"mm\",\"time\":\"s\",\"frame\":\"device: origin top-left, +X right, +Y down\"},\n",f);
    settings(f,&p->settings);
    fprintf(f,"\"duration\":%.17g,\"simulated_seconds\":%.17g,\"steps\":[%llu,%llu],"
        "\"reversals\":[%llu,%llu],\"start\":[%.17g,%.17g],\n\"erases\":[",
        t->clock,seconds,(unsigned long long)p->total_steps[0],(unsigned long long)p->total_steps[1],
        (unsigned long long)p->reversals[0],(unsigned long long)p->reversals[1],t->start_mm.x,t->start_mm.y);
    for (size_t i=0;i<t->erase_count;i++)
        fprintf(f,"%s[%.17g,%.17g]",i ? "," : "",t->erases[i].t0,t->erases[i].duration);
    fputs("],\n",f); segments(f,t);
    fputs("\"requirements\":",f);
    if (!MachineRequirementsFramesWrite(f,frames,1)) return 0;
    fputs(",\n\"gcode\":",f); string(f,t->source); fputs("}\n",f);
    return !ferror(f);
}
int MachineTraceWriteWithRequirements(const char *filename,const MachineStepProgram *p,
    const EtchTrace *t,double seconds,const MachineRequirementsFrames *frames,MachineParseError *e)
{
    if (!filename || !*filename || !valid(p,t,seconds)) return fail(e,"invalid or incomplete trace output");
    if (!MachineRequirementsFramesValid(frames) || t->erase_count==SIZE_MAX ||
        frames->count!=t->erase_count+1 || frames->knob_mm_turn!=p->settings.knob_mm_turn)
        return fail(e,"invalid trace frame requirements");
    size_t length=strlen(filename);
    if (length>SIZE_MAX-12) return fail(e,"trace output filename too long");
    char *temporary=malloc(length+12);
    if (!temporary) return fail(e,"cannot allocate trace output filename");
    snprintf(temporary,length+12,"%s.tmp.XXXXXX",filename);
    int fd=mkstemp(temporary);
    if (fd<0) { free(temporary); return fail(e,"cannot create trace output"); }
    FILE *stream=fdopen(fd,"wb");
    if (!stream) { close(fd); unlink(temporary); free(temporary); return fail(e,"cannot open trace output"); }
    int ok=document(stream,p,t,seconds,frames) && !ferror(stream);
    if (fclose(stream)) ok=0;
    if (ok && rename(temporary,filename)) ok=0;
    if (!ok) unlink(temporary);
    free(temporary);
    if (!ok) return fail(e,"cannot publish trace output");
    if (e) *e=(MachineParseError){0};
    return 1;
}
