// SPDX-License-Identifier: GPL-2.0-or-later
#include "run.h"
#include <assert.h>

/* Expected positions come from actual untraced playback, never trace sampling. */
int main(int argc, char **argv)
{
    assert(argc >= 4);
    const char *const *assignments = (const char *const *)(argv + 4);
    size_t count = (size_t)(argc - 4);
    MachineParseError error = {0};
    EtchTrace trace = {0};
    MachinePlayback *recorded = MachineLoadTraced(argv[1], assignments, count, &trace, &error);
    assert(recorded && MachineRun(recorded));
    double duration = MachinePlaybackSeconds(recorded);
    assert(MachineTraceWrite(argv[2], MachinePlaybackProgram(recorded), &trace, duration, &error));
    MachinePlaybackDestroy(recorded);
    EtchTraceFree(&trace);
    FILE *samples = fopen(argv[3], "wb");
    assert(samples);
    fputs("[", samples);
    unsigned intervals = duration > 0 ? 2000 : 0;
    for (unsigned i = 0; i <= intervals; ++i) {
        double time = intervals ? duration * i / intervals : 0;
        MachinePlayback *plain = MachineLoad(argv[1], assignments, count, &error);
        assert(plain && MachinePlaybackTick(plain, time));
        EtchStatus status = EtchDeviceGetStatus(MachinePlaybackDevice(plain));
        fprintf(samples, "%s[%.17g,%.17g,%.17g,%.17g,%.17g]", i ? ",\n" : "\n",
                time, status.cursor_mm.x, status.cursor_mm.y, status.motor_mm.x, status.motor_mm.y);
        MachinePlaybackDestroy(plain);
    }
    assert(fputs("]\n", samples) >= 0);
    assert(!fclose(samples));
    return 0;
}
