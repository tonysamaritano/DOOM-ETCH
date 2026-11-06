// SPDX-License-Identifier: GPL-2.0-or-later
#include "machine_playback_test.h"
#include "trace.h"
#include "run.h"
#include <unistd.h>
static void unchanged(const char *path)
{
    FILE *f = fopen(path, "rb"); assert(f); char text[32] = {0};
    assert(fread(text, 1, sizeof(text), f) == 9); assert(!fclose(f));
    assert(!strcmp(text, "sentinel\n"));
}
int main(void)
{
    FILE *source = tmpfile(); assert(source);
    fputs("; ETCH_START X0 Y127.6\nG1 X100 F3600\nG1 X50\nM2\n", source); rewind(source);
    MachineSettings settings; MachineSettingsDefault(&settings);
    MachineProgram parsed = {0}; MachineStepProgram program = {0}; MachineParseError error = {0};
    assert(MachineRead(source, &settings, &parsed, &error)); assert(!fclose(source));
    assert(MachineSteppersCompile(&program, &parsed, &settings, &error));
    EtchTrace trace = {0};
    MachinePlayback *p = MachinePlaybackCreateTraced(&program, &trace, &error); assert(p);
    char path[] = "/tmp/doom-etch-trace-write.XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0);
    assert(write(fd, "sentinel\n", 9) == 9); assert(!close(fd));
    assert(!MachineTraceWrite(path, &program, &trace, MachinePlaybackSeconds(p), &error));
    assert(error.message[0]); unchanged(path);
    assert(MachineRun(p)); double seconds = MachinePlaybackSeconds(p);
    assert(!MachineTraceWrite(path, &program, &trace, seconds + .125, &error));
    assert(error.message[0]); unchanged(path);
    assert(!MachineTraceWrite(path, &program, &trace, NAN, &error)); unchanged(path);
    trace.clock += .125;
    assert(!MachineTraceWrite(path, &program, &trace, seconds, &error)); unchanged(path);
    trace.clock -= .125;
    double duration = trace.segments[0].duration;
    trace.segments[0].duration = duration - .001; // A real gap before the next move.
    assert(!MachineTraceWrite(path, &program, &trace, seconds, &error)); unchanged(path);
    trace.segments[0].duration = duration + .001; // A real overlap before the next move.
    assert(!MachineTraceWrite(path, &program, &trace, seconds, &error)); unchanged(path);
    trace.segments[0].duration = duration;
    trace.failed = 1;
    assert(!MachineTraceWrite(path, &program, &trace, seconds, &error)); unchanged(path);
    trace.failed = 0;
    assert(MachineTraceWrite(path, &program, &trace, seconds, &error));
    FILE *result = fopen(path, "rb"); assert(result); assert(fgetc(result) == '{');
    assert(!fclose(result)); assert(!unlink(path));
    MachinePlaybackDestroy(p); EtchTraceFree(&trace);
    MachineStepProgramFree(&program); MachineProgramFree(&parsed);
    puts("machine-trace-write: incomplete and inconsistent traces fail without replacing output");
    return 0;
}
