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

static void playback_partition(void)
{
    MachineSettings sa, sb; MachineSettingsDefault(&sa); sb = sa;
    const char *text = "; ETCH_START X0 Y127.6\nF3600\nG1 X100\n"
        "F1200\nY27.6\nM0\nG0 X0\nM2";
    MachinePlayback *a = play_text(text, &sa), *b = play_text(text, &sb);
    double before = MachinePlaybackSeconds(a);
    assert(!MachinePlaybackTick(a, NAN)); assert(!MachinePlaybackTick(a, -1));
    machine_near(MachinePlaybackSeconds(a), before);
    assert(MachinePlaybackTick(a, 30));
    for (int i = 0; i < 30000; ++i) assert(MachinePlaybackTick(b, .001));
    assert(MachinePlaybackDone(a) && MachinePlaybackDone(b));
    machine_near(MachinePlaybackSeconds(a), MachinePlaybackSeconds(b));
    compare_devices(MachinePlaybackDevice(a), MachinePlaybackDevice(b));
    double finished = MachinePlaybackSeconds(a);
    assert(MachinePlaybackTick(a, 100)); machine_near(MachinePlaybackSeconds(a), finished);
    MachinePlaybackDestroy(a); MachinePlaybackDestroy(b);
}

static void zero_motion_program(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    MachinePlayback *p = play_text("; ETCH_START X5 Y122.6\n"
        "F3600\nG1 X5 Y122.6\nM0\nG1 X5 Y122.6\nM2", &s);
    assert(MachinePlaybackTick(p, 100));
    assert(MachinePlaybackDone(p));
    machine_near(MachinePlaybackSeconds(p), 2.5);
    EtchDevice *d = MachinePlaybackDevice(p);
    machine_point(EtchDeviceGetStatus(d).cursor_mm, 5, 5);
    assert(ink_count(d, ETCH_INK_FRESH) == 0);
    assert(ink_count(d, ETCH_INK_GHOST) == 0);
    MachinePlaybackDestroy(p);
}

static void cumulative_wall_clock(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    s.max_rate_mm_min[0] = 6e-9;
    s.max_rate_mm_min[1] = 6e151; s.acceleration_mm_s2[1] = 1e300;
    MachinePlayback *p = play_text("; ETCH_START X0 Y127.6\n"
        "F0.000000006\nG1 X100\nG0 Y127.55\nM2", &s);
    assert(!MachinePlaybackTick(p, 1e13));
    const MachineParseError *e = MachinePlaybackError(p);
    assert(e && e->message[0]);
    double failed_clock = MachinePlaybackSeconds(p);
    assert(!MachinePlaybackTick(p, 1));
    machine_near(MachinePlaybackSeconds(p), failed_clock);
    MachinePlaybackDestroy(p);
}

int main(void)
{
    shake_clock(); playback_partition(); zero_motion_program(); cumulative_wall_clock();
    puts("machine-playback: copied program, M0 clock and chunk invariance passed");
    return 0;
}
