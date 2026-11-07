// SPDX-License-Identifier: GPL-2.0-or-later
#include "run.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static int source_alias(const char *input,const char *output)
{
    struct stat a,b;
    return !stat(input,&a) && !stat(output,&b) && a.st_dev==b.st_dev && a.st_ino==b.st_ino;
}
int main(int argc,char **argv)
{
    if (argc<2 || argv[1][0]=='-') {
        fputs("Usage: etch-gcode-run FILE [--settings k=v ...] [--machine-readable] [--trace out.json] [--target-fps N]\n",stderr);
        return 2;
    }
    const char **settings=calloc((size_t)argc,sizeof(*settings));
    if (!settings) return 1;
    size_t count=0; int json=0; const char *invalid=NULL,*tracefile=NULL;
    double fps=30; int requested_fps=0;
    for (int i=2;i<argc;i++) {
        if (!strcmp(argv[i],"--machine-readable")) json=1;
        else if (!strcmp(argv[i],"--target-fps")) {
            if (requested_fps || i+1==argc || !MachineRequirementsParseFPS(argv[i+1],&fps))
                invalid="--target-fps";
            else { requested_fps=1; i++; }
        }
        else if (!strcmp(argv[i],"--trace")) {
            if (tracefile || i+1==argc || !*argv[i+1] || !strncmp(argv[i+1],"--",2)) invalid="--trace";
            else tracefile=argv[++i];
        } else if (!strcmp(argv[i],"--settings")) {
            size_t before=count;
            while (i+1<argc && strncmp(argv[i+1],"--",2)) settings[count++]=argv[++i];
            if (count==before) invalid="--settings";
        } else if (!invalid) invalid=argv[i];
    }
    if (invalid) {
        fprintf(stderr,"Invalid option or missing setting: %s\n",invalid);
        free(settings); return 2;
    }
    if (tracefile && source_alias(argv[1],tracefile)) {
        fputs("Trace output must not alias the G-code source\n",stderr); free(settings); return 2;
    }
    MachineParseError error={0};
    EtchTrace trace={0};
    MachinePlayback *play=MachineLoadTraced(argv[1],settings,count,tracefile ? &trace : NULL,&error);
    free(settings);
    if (!play) {
        fprintf(stderr,"%s:%zu: %s\n",argv[1],error.line,error.message);
        EtchTraceFree(&trace); return 1;
    }
    const MachineStepProgram *program=MachinePlaybackProgram(play);
    MachineRequirementsFrames requirements={0};
    int ok=!(requested_fps || tracefile) ||
        MachineRequirementsFramesBuild(&requirements,program,fps,&error);
    if (ok) {
        ok=MachineRun(play);
        if (!ok) error=*MachinePlaybackError(play);
    }
    if (ok && tracefile) ok=MachineTraceWriteWithRequirements(tracefile,program,&trace,
        MachinePlaybackSeconds(play),&requirements,&error);
    if (ok) ok=requested_fps ? MachineReportWithRequirements(stdout,program,
        MachinePlaybackSeconds(play),json,&requirements) :
        MachineReport(stdout,program,MachinePlaybackSeconds(play),json);
    else fprintf(stderr,"%s:%zu: %s\n",argv[1],error.line,error.message);
    MachineRequirementsFramesFree(&requirements);
    MachinePlaybackDestroy(play); EtchTraceFree(&trace);
    return ok ? 0 : 1;
}
