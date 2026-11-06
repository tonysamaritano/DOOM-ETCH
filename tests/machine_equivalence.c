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
