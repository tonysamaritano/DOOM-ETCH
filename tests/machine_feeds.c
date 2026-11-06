#include "machine_playback_test.h"

static void feeds_and_hardware(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    s.max_rate_mm_min[1] = 60; s.acceleration_mm_s2[1] = 1;
    MachinePlayback *p = play_text("; ETCH_START X0 Y127.6\n"
        "F600\nG1 X10\nF1200\nX20\nG0 X120\nM2", &s);
    assert(MachinePlaybackTick(p, 1000));
    assert(MachinePlaybackDone(p));
    double expected = (10.0 / 10 + 10.0 / 250) +
        (10.0 / 20 + 20.0 / 250) + (100.0 / 60 + 60.0 / 250);
    machine_near(MachinePlaybackSeconds(p), expected);
    machine_point(EtchDeviceGetStatus(MachinePlaybackDevice(p)).cursor_mm, 120, 0);
    MachinePlaybackDestroy(p);
}

static void diagonal_feed(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    MachinePlayback *p = play_text("; ETCH_START X0 Y127.6\n"
        "G1 X100 Y27.6 F3600\nM2", &s);
    assert(MachinePlaybackTick(p, 10));
    machine_near(MachinePlaybackSeconds(p), 134 * sqrt(2) / 75);
    MachinePlaybackDestroy(p);
}

int main(void)
{
    feeds_and_hardware(); diagonal_feed();
    puts("machine-feeds: feed changes, hardware G0 and per-axis limits passed");
    return 0;
}
