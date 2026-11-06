#include "machine_playback_test.h"

static MachinePlayback *two_frames(void)
{
    unsigned char kinds[] = {MOTION_START, MOTION_OUTLINE, MOTION_CONNECTOR};
    MotionPoint a[] = {{100, 100}, {800, 100}, {800, 800}};
    MotionPoint b[] = {{800, 800}, {200, 800}, {200, 200}};
    MotionPath first = {a, kinds, 3, 3403, 2552, 170.15, 127.6};
    MotionPath second = first; second.points = b;
    FILE *f = tmpfile(); assert(f);
    assert(MotionWriteGCodeFrame(f, &first, 3600, 1, 0));
    assert(MotionWriteGCodeFrame(f, &second, 3600, 0, 1)); rewind(f);
    MachineSettings s; MachineSettingsDefault(&s);
    MachineProgram p = {0}; MachineStepProgram steps = {0}; MachineParseError e = {0};
    assert(MachineRead(f, &s, &p, &e)); fclose(f);
    assert(MachineSteppersCompile(&steps, &p, &s, &e));
    MachinePlayback *play = MachinePlaybackCreate(&steps, &e); assert(play);
    MachineProgramFree(&p); MachineStepProgramFree(&steps);
    return play;
}
