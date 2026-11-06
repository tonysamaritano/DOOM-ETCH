#import "../platform/darwin/file_play.m"
#include "machine_test.h"

/* These guards make accidental window/event use fail rather than open UI. */
NSWindow *I_MetalFileOpen(void) { assert(0); return nil; }
BOOL I_MetalFileClosed(void) { assert(0); return YES; }
void I_MetalFileDraw(const unsigned char *ink, double fade)
{ (void)ink; (void)fade; assert(0); }
unsigned I_MetalPresentedFrames(void) { assert(0); return 0; }
void I_ShutdownGraphics(void) { assert(0); }
int DoomKeyForEvent(NSEvent *event) { (void)event; assert(0); return 0; }

static Presentation fixture(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    MachineParseOptions options = {{0, 127.6}};
    MachineProgram p = {0}; MachineStepProgram steps = {0}; MachineParseError e = {0};
    assert(MachineGCodeParse(&p, "F3600\nG1 X100 Y127.6\nM2", &options, &e));
    assert(MachineSteppersCompile(&steps, &p, &s, &e));
    MachinePlayback *play = MachinePlaybackCreate(&steps, &e); assert(play);
    MachineProgramFree(&p); MachineStepProgramFree(&steps);
    return (Presentation){.play = play, .rate = 1000};
}

static void pause_and_interrupt(void)
{
    Presentation p = fixture();
    assert(advance(&p, .2));
    assert(control(&p, KEY_F9) && p.paused && p.reset_clock);
    assert(advance(&p, 5)); machine_near(MachinePlaybackSeconds(p.play), .2);
    assert(control(&p, KEY_F9) && !p.paused && p.reset_clock);
    assert(advance(&p, .1)); machine_near(MachinePlaybackSeconds(p.play), .3);
    unsigned char physical[sizeof p.retained];
    memcpy(physical, EtchDeviceInk(MachinePlaybackDevice(p.play)), sizeof physical);
    assert(control(&p, KEY_F7) && p.interrupted && p.shaking);
    assert(!memcmp(p.retained, physical, sizeof physical));
    assert(advance(&p, .5)); machine_near(p.shake_time, .5);
    assert(control(&p, KEY_F7)); machine_near(p.shake_time, .5);
    assert(control(&p, KEY_F9) && advance(&p, 10)); machine_near(p.shake_time, .5);
    assert(control(&p, KEY_F9) && advance(&p, 2));
    assert(p.shaken && !p.shaking && !p.reported);
    machine_near(MachinePlaybackSeconds(p.play), .3);
    assert(!memcmp(physical, EtchDeviceInk(MachinePlaybackDevice(p.play)), sizeof physical));
    size_t marks = 0, ghosts = 0;
    for (size_t i = 0; i < sizeof physical; ++i) {
        marks += physical[i] != 0; ghosts += p.retained[i] == ETCH_INK_GHOST;
        assert(p.retained[i] == 0 || p.retained[i] == ETCH_INK_GHOST);
    }
    assert(marks > 0 && ghosts == (size_t)floor(marks * .03));
    assert(control(&p, KEY_F7) && !p.shaking && p.shaken);
    MachinePlaybackDestroy(p.play);
}
