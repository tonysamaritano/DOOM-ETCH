#include "machine_playback_test.h"

static MachinePlayback *serialized(const MotionPath *path)
{
    FILE *f = tmpfile(); assert(f);
    assert(MotionWriteGCode(f, path, 3600)); rewind(f);
    MachineSettings s; MachineSettingsDefault(&s);
    MachineProgram p = {0}; MachineStepProgram steps = {0}; MachineParseError e = {0};
    assert(MachineRead(f, &s, &p, &e)); fclose(f);
    assert(MachineSteppersCompile(&steps, &p, &s, &e));
    MachinePlayback *play = MachinePlaybackCreate(&steps, &e); assert(play);
    MachineProgramFree(&p); MachineStepProgramFree(&steps);
    return play;
}

static EtchDevice *raw_direct(const MotionPath *path)
{
    assert(path->count <= 16);
    MotionPoint mm[16];
    for (size_t i = 0; i < path->count; ++i) {
        mm[i].x = (path->points[i].x / path->width) * path->width_mm;
        mm[i].y = (path->points[i].y / path->height) * path->height_mm;
    }
    EtchConfig c = EtchAssumedDefaults();
    c.width_mm = path->width_mm; c.height_mm = path->height_mm;
    EtchDevice *d = EtchCreate(c, mm[0]); assert(d);
    assert(EtchDeviceSubmit(d, mm, path->count, 60));
    return d;
}

static void equivalent(MotionPath path, double expected_seconds)
{
    assert(MotionValid(&path));
    EtchDevice *direct = raw_direct(&path);
    MachinePlayback *play = serialized(&path);
    double duration = EtchDeviceGetStatus(direct).remaining_s;
    if (expected_seconds >= 0) machine_near(duration, expected_seconds);
    for (int i = 0; i < 101; ++i) {
        double dt = duration / 100;
        assert(EtchDeviceTick(direct, dt));
        assert(MachinePlaybackTick(play, dt));
        compare_devices(direct, MachinePlaybackDevice(play));
    }
    assert(EtchDeviceReady(direct) && MachinePlaybackDone(play));
    machine_near(MachinePlaybackSeconds(play), duration);
    machine_near(EtchDeviceGetStatus(direct).elapsed_s, duration);
    printf("machine-equivalence points=%zu seconds=%.17g\n", path.count, duration);
    EtchDestroy(direct); MachinePlaybackDestroy(play);
}

static void real_size_paths(void)
{
    unsigned char kinds[] = {MOTION_START, MOTION_OUTLINE, MOTION_CONNECTOR,
        MOTION_OUTLINE, MOTION_OUTLINE, MOTION_CONNECTOR};
    MotionPoint diagonal[] = {{0, 0}, {2000, 2000}};
    MotionPath p = {diagonal, kinds, 2, 3403, 2552, 170.15, 127.6};
    equivalent(p, 134 * sqrt(2) / 75);
    MotionPoint reverse[] = {{100, 100}, {2100, 100}, {2100, 100},
        {100, 100}, {100, 2100}, {100, 100}};
    p.points = reverse; p.count = 6;
    equivalent(p, 2 * (100 / 60.0 + 60.0 / 250) +
        2 * (100.15 / 60.0 + 60.0 / 250));
}

static void raw_ties_and_extents(void)
{
    unsigned char kinds[] = {MOTION_START, MOTION_OUTLINE, MOTION_OUTLINE,
        MOTION_CONNECTOR, MOTION_OUTLINE};
    MotionPoint tie[] = {{0, 0}, {.49999998, .49999998}, {.5, .5},
        {.50000002, .50000002}, {20, 20}};
    MotionPath p = {tie, kinds, 5, 3403, 2552, 170.15, 127.6};
    equivalent(p, -1);
    MotionPoint exact_ties[] = {{0, 0}, {.025, .025}, {.075, .075}, {.125, .125}};
    p = (MotionPath){exact_ties, kinds, 4, 100, 100, 100, 100};
    equivalent(p, -1);
    MotionPoint custom[] = {{10, 10}, {100, 100}, {0, 0}, {50, 50}};
    p = (MotionPath){custom, kinds, 4, 100, 100, 1.023, 1.037};
    equivalent(p, -1);
}

int main(void)
{
    real_size_paths(); raw_ties_and_extents();
    puts("machine-equivalence: raw IR and serialized controller agree");
    return 0;
}
