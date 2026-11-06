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
