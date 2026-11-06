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
        fputs("Usage: etch-gcode-run FILE [--settings k=v ...] [--machine-readable] [--trace out.json]\n",stderr);
        return 2;
    }
    const char **settings=calloc((size_t)argc,sizeof(*settings));
    if (!settings) return 1;
    size_t count=0; int json=0; const char *invalid=NULL,*tracefile=NULL;
    for (int i=2;i<argc;i++) {
        if (!strcmp(argv[i],"--machine-readable")) json=1;
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
    MachinePlayback *play=MachineLoad(argv[1],settings,count,&error);
    free(settings);
    if (!play) { fprintf(stderr,"%s:%zu: %s\n",argv[1],error.line,error.message); return 1; }
    int ok=MachineRun(play);
    if (ok) ok=MachineReport(stdout,MachinePlaybackProgram(play),MachinePlaybackSeconds(play),json);
    else {
        const MachineParseError *failure=MachinePlaybackError(play);
        fprintf(stderr,"%s:%zu: %s\n",argv[1],failure->line,failure->message);
    }
    MachinePlaybackDestroy(play);
    return ok ? 0 : 1;
}
