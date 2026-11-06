#include "machine_playback_test.h"

static size_t ink_count(EtchDevice *d, unsigned char kind)
{
    const unsigned char *ink = EtchDeviceInk(d); size_t n = 0;
    for (size_t i = 0; i < ETCH_INK_WIDTH * ETCH_INK_HEIGHT; ++i) n += ink[i] == kind;
    return n;
}

static void shake_clock(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    MachinePlayback *p = play_text("; ETCH_START X0 Y127.6\nF3600\n"
        "G1 X100\nM0\nX0\nM2", &s);
    EtchDevice *d = MachinePlaybackDevice(p);
    double outward = 143.0 / 75;
    assert(MachinePlaybackTick(p, outward));
    size_t fresh = ink_count(d, ETCH_INK_FRESH); assert(fresh > 100);
    assert(MachinePlaybackTick(p, .5));
    EtchStatus status = EtchDeviceGetStatus(d);
    assert(status.erasing); machine_near(status.erase_left_s, 2);
    machine_point(status.cursor_mm, 100, 0);
    machine_near(MachinePlaybackSeconds(p), outward + .5);
    assert(MachinePlaybackTick(p, 2));
    assert(ink_count(d, ETCH_INK_GHOST) == (size_t)floor(fresh * .03));
    assert(MachinePlaybackTick(p, 100));
    assert(MachinePlaybackDone(p));
    machine_near(MachinePlaybackSeconds(p), outward + 2.5 + 100.15 / 60 + .24);
    machine_point(EtchDeviceGetStatus(d).cursor_mm, 0, 0);
    MachinePlaybackDestroy(p);
}
