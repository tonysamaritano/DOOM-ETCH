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
    assert(control(&p, KEY_F7) && p.shaking);
    assert(advance(&p, 2.5) && p.shaken);
    size_t twice = 0;
    for (size_t i = 0; i < sizeof p.retained; ++i) twice += p.retained[i] != 0;
    assert(twice == (size_t)floor(ghosts * .03));
    machine_near(MachinePlaybackSeconds(p.play), .3);
    MachinePlaybackDestroy(p.play);
}

static void replay_and_completed_shake(void)
{
    Presentation p = fixture();
    assert(control(&p, '+') && p.rate == 1024);
    assert(advance(&p, .2));
    assert(control(&p, KEY_F9) && control(&p, KEY_F7));
    assert(control(&p, KEY_F6));
    assert(p.rate == 1024 && !p.paused && !p.interrupted && !p.shaking && !p.reported);
    assert(p.reset_clock && p.shake_time == 0);
    machine_near(MachinePlaybackSeconds(p.play), 0);
    for (size_t i = 0; i < sizeof p.retained; ++i) {
        assert(p.retained[i] == 0);
        assert(EtchDeviceInk(MachinePlaybackDevice(p.play))[i] == 0);
    }
    assert(advance(&p, 2)); assert(p.reported && MachinePlaybackDone(p.play));
    double physical = MachinePlaybackSeconds(p.play);
    assert(control(&p, KEY_F7) && advance(&p, 2.5));
    assert(p.shaken && p.reported);
    machine_near(MachinePlaybackSeconds(p.play), physical);
    assert(control(&p, KEY_F6) && !p.reported && !p.interrupted && p.rate == 1024);
    MachinePlaybackDestroy(p.play);
}

int main(void)
{
    @autoreleasepool {
        FILE *captured = tmpfile(); assert(captured);
        int saved = dup(STDOUT_FILENO); assert(saved >= 0);
        fflush(stdout); assert(dup2(fileno(captured), STDOUT_FILENO) >= 0);
        pause_and_interrupt();
        fflush(stdout); assert(ftell(captured) == 0);
        assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(captured);
        replay_and_completed_shake();
    }
    puts("Actual file presentation controls passed without UI");
    return 0;
}
