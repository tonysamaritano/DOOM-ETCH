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

int main(void)
{
    MachinePlayback *play = two_frames();
    MotionPoint a[] = {{5, 5}, {40, 5}, {40, 40}};
    MotionPoint b[] = {{40, 40}, {10, 40}, {10, 10}};
    EtchDevice *direct = EtchCreate(EtchAssumedDefaults(), a[0]); assert(direct);
    assert(EtchDeviceSubmit(direct, a, 3, 60));
    double first = EtchDeviceGetStatus(direct).remaining_s;
    assert(EtchDeviceTick(direct, first));
    assert(MachinePlaybackTick(play, first));
    compare_devices(direct, MachinePlaybackDevice(play));
    assert(EtchDeviceRequestErase(direct));
    assert(EtchDeviceTick(direct, 2.5));
    assert(MachinePlaybackTick(play, 2.5));
    compare_devices(direct, MachinePlaybackDevice(play));
    assert(EtchDeviceSubmit(direct, b, 3, 60));
    double second = EtchDeviceGetStatus(direct).remaining_s;
    assert(EtchDeviceTick(direct, second));
    assert(MachinePlaybackTick(play, second));
    compare_devices(direct, MachinePlaybackDevice(play));
    assert(MachinePlaybackDone(play));
    machine_near(MachinePlaybackSeconds(play), first + 2.5 + second);
    machine_near(first, 2 * (35.0 / 60 + .24));
    machine_near(second, 2 * (30.15 / 60 + .24));
    EtchDestroy(direct); MachinePlaybackDestroy(play);
    puts("machine-multiframe: actual stream matches direct motion and timed shake");
    return 0;
}
