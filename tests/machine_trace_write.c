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
    fputs("; ETCH_START X0 Y127.6\nG1 X100 F3600\nM2\n", source); rewind(source);
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
