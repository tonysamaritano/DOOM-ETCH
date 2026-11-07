// SPDX-License-Identifier: GPL-2.0-or-later
#include "run.h"
#include <inttypes.h>
#include <math.h>
static int fail(MachineParseError *e,const char *message)
{
    if (e) { e->line=0; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
MachinePlayback *MachineLoad(const char *filename,const char *const *assignments,
                             size_t count,MachineParseError *e)
{
    return MachineLoadTraced(filename,assignments,count,NULL,e);
}
MachinePlayback *MachineLoadTraced(const char *filename,const char *const *assignments,
                                   size_t count,EtchTrace *trace,MachineParseError *e)
{
    if (!filename || (count && !assignments)) { fail(e,"missing file or settings"); return NULL; }
    if (trace && (trace->source || trace->segments || trace->erases || trace->failed || trace->completed)) {
        fail(e,"trace must be empty"); return NULL;
    }
    FILE *stream=fopen(filename,"rb");
    if (!stream) { fail(e,"cannot open G-code file"); return NULL; }
    if (trace) {
        FILE *copy=EtchTraceSource(stream,trace,e);
        int closed=fclose(stream);
        if (!copy) return NULL;
        if (closed) { fclose(copy); fail(e,"cannot close G-code source"); return NULL; }
        stream=copy;
    }
    MachineSettings settings; MachineSettingsDefault(&settings);
    MachineProgram parsed={0}; MachineStepProgram compiled={0};
    int ok=MachineRead(stream,&settings,&parsed,e);
    if (fclose(stream) && ok) ok=fail(e,"cannot close G-code file");
    for (size_t i=0;ok && i<count;i++) ok=MachineSettingsParse(&settings,assignments[i],e);
    if (ok) ok=MachineSteppersCompile(&compiled,&parsed,&settings,e);
    MachinePlayback *play=ok ? MachinePlaybackCreateTraced(&compiled,trace,e) : NULL;
    MachineProgramFree(&parsed); MachineStepProgramFree(&compiled);
    return play;
}
int MachineRun(MachinePlayback *play)
{
    if (!play) return 0;
    while (!MachinePlaybackDone(play)) {
        EtchStatus status=EtchDeviceGetStatus(MachinePlaybackDevice(play));
        double left=status.erasing ? status.erase_left_s : status.remaining_s;
        if (!MachinePlaybackTick(play,left)) return 0;
    }
    return 1;
}
static int calibration(FILE *f,const MachineSettings *s)
{
    return fprintf(f,"\"calibration\":{\"steps_per_mm\":[%.17g,%.17g],"
        "\"rate_mm_min\":[%.17g,%.17g],\"acceleration_mm_s2\":[%.17g,%.17g],"
        "\"travel_mm\":[%.17g,%.17g],\"backlash_mm\":%.17g,\"erase_seconds\":%.17g,"
        "\"ghost_fraction\":%.17g,\"knob_mm_turn\":%.17g}",
        s->steps_per_mm[0],s->steps_per_mm[1],s->max_rate_mm_min[0],s->max_rate_mm_min[1],
        s->acceleration_mm_s2[0],s->acceleration_mm_s2[1],s->width_mm,s->height_mm,
        s->backlash_mm,s->erase_seconds,s->ghost_fraction,s->knob_mm_turn)>0;
}
static int report(FILE *f,const MachineStepProgram *p,double seconds,int json,
                  const MachineRequirementsFrames *requirements)
{
    if (!f || !p || !isfinite(seconds) || seconds<0 ||
        !MachineSettingsValidate(&p->settings,NULL)) return 0;
    size_t moves=0,erases=0;
    for (size_t i=0;i<p->count;i++) {
        moves+=p->moves[i].kind==MACHINE_MOVE_DRAW || p->moves[i].kind==MACHINE_MOVE_RAPID;
        erases+=p->moves[i].kind==MACHINE_MOVE_PAUSE;
    }
    if (requirements && (!MachineRequirementsFramesValid(requirements) ||
        requirements->count!=erases+1 || requirements->knob_mm_turn!=p->settings.knob_mm_turn)) return 0;
    if (json) {
        fprintf(f,"{\"type\":\"machine-summary\",\"moves\":%zu,\"erases\":%zu,"
            "\"steps_x\":%" PRIu64 ",\"steps_y\":%" PRIu64 ","
            "\"reversals_x\":%" PRIu64 ",\"reversals_y\":%" PRIu64 ","
            "\"simulated_seconds\":%.17g,\"assumed\":true,",moves,erases,
            p->total_steps[0],p->total_steps[1],p->reversals[0],p->reversals[1],seconds);
        if (!calibration(f,&p->settings)) return 0;
        if (requirements) {
            fputs(",\"requirements\":",f);
            if (!MachineRequirementsFramesWrite(f,requirements,1)) return 0;
        }
        fputs("}\n",f);
    } else {
        fprintf(f,"Completed file: %zu moves, %zu erases, %.17g simulated seconds\n"
            "Steps X/Y: %" PRIu64 "/%" PRIu64 "; reversals X/Y: %" PRIu64 "/%" PRIu64 "\n",
            moves,erases,seconds,p->total_steps[0],p->total_steps[1],p->reversals[0],p->reversals[1]);
        fputs("ASSUMED calibration: ",f); if (!calibration(f,&p->settings)) return 0; fputc('\n',f);
        if (requirements && !MachineRequirementsFramesWrite(f,requirements,0)) return 0;
    }
    return !ferror(f) && fflush(f)==0;
}
int MachineReport(FILE *f,const MachineStepProgram *p,double seconds,int json)
{
    return report(f,p,seconds,json,NULL);
}
int MachineReportWithRequirements(FILE *f,const MachineStepProgram *p,double seconds,
                                 int json,const MachineRequirementsFrames *requirements)
{
    return requirements && report(f,p,seconds,json,requirements);
}
