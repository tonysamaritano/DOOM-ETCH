#ifndef DOOM_MACHINE_PLAYBACK_TEST_H
#define DOOM_MACHINE_PLAYBACK_TEST_H
#include "playback.h"
#include "machine_test.h"

static inline MachinePlayback *play_text(const char *text, MachineSettings *settings)
{
    FILE *f = tmpfile(); assert(f);
    assert(fputs(text, f) >= 0); rewind(f);
    MachineProgram p = {0}; MachineStepProgram steps = {0}; MachineParseError e = {0};
    assert(MachineRead(f, settings, &p, &e)); fclose(f);
    assert(MachineSteppersCompile(&steps, &p, settings, &e));
    MachinePlayback *play = MachinePlaybackCreate(&steps, &e); assert(play);
    MachineProgramFree(&p); MachineStepProgramFree(&steps);
    return play;
}

static inline void compare_devices(EtchDevice *a, EtchDevice *b)
{
    EtchStatus x = EtchDeviceGetStatus(a), y = EtchDeviceGetStatus(b);
    machine_point(x.cursor_mm, y.cursor_mm.x, y.cursor_mm.y);
    machine_point(x.motor_mm, y.motor_mm.x, y.motor_mm.y);
    machine_point(x.motor_velocity_mm_s, y.motor_velocity_mm_s.x, y.motor_velocity_mm_s.y);
    assert(!memcmp(EtchDeviceInk(a), EtchDeviceInk(b), ETCH_INK_WIDTH * ETCH_INK_HEIGHT));
}
#endif
