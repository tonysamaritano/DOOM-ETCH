// SPDX-License-Identifier: GPL-2.0-or-later
#include "machine_playback_test.h"
#include "trace.h"
#include "run.h"
static const char *fixture =
    "; ETCH_SIZE X170.15 Y127.6\n; ETCH_START X0 Y127.6\n"
    "G21\nG90\nF3600\nG1 X100\nG1 X40 Y97.6\n"
    "F600\nG1 X50 Y87.6\nM0\nG0 X10 Y117.6\n"
    "G1 X10 Y117.6\nM0\nM2\n";
static MachinePlayback *record(const char *text, MachineSettings settings, EtchTrace *trace)
{
    FILE *f = tmpfile(); assert(f); assert(fputs(text, f) >= 0); rewind(f);
    MachineProgram parsed = {0}; MachineStepProgram compiled = {0}; MachineParseError error = {0};
    assert(MachineRead(f, &settings, &parsed, &error)); assert(!fclose(f));
    assert(MachineSteppersCompile(&compiled, &parsed, &settings, &error));
    MachinePlayback *p = MachinePlaybackCreateTraced(&compiled, trace, &error); assert(p);
    MachineProgramFree(&parsed); MachineStepProgramFree(&compiled);
    assert(MachineRun(p)); assert(MachinePlaybackDone(p));
    machine_near(trace->clock, MachinePlaybackSeconds(p));
    return p;
}
static void near_sample(MotionPoint actual, MotionPoint expected)
{
    assert(isfinite(actual.x) && isfinite(actual.y));
    if (fabs(actual.x - expected.x) > 1e-5 || fabs(actual.y - expected.y) > 1e-5) {
        fprintf(stderr, "trace (%.17g,%.17g), device (%.17g,%.17g)\n",
                actual.x, actual.y, expected.x, expected.y);
        abort();
    }
}
