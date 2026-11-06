#include "steppers.h"
#include "machine_test.h"

static void counters(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    MachineProgram p = {0}; MachineStepProgram steps = {0}; MachineParseError e = {0};
    MachineParseOptions options = {{1, 2}};
    assert(MachineGCodeParse(&p, "G1 X3 Y2 F3600\nY1\nY1\nM0\nX1\nM2", &options, &e));
    assert(MachineSteppersCompile(&steps, &p, &s, &e));
    assert(steps.count == 6 && steps.start_steps[0] == 20);
    assert(steps.start_steps[1] == 2512); /* (127.6 - 2) * 20 */
    assert(steps.final_steps[0] == 20 && steps.final_steps[1] == 2532);
    assert(steps.total_steps[0] == 80 && steps.total_steps[1] == 20);
    assert(steps.reversals[0] == 1 && steps.reversals[1] == 0);
    for (size_t i = 0; i < steps.count; ++i) assert(steps.moves[i].line == i + 1);
    assert(steps.moves[2].target_steps[1] == 2532);
    assert(steps.moves[3].kind == MACHINE_MOVE_PAUSE);
    machine_near(steps.moves[0].feed_mm_s, 60);
    MachineProgramFree(&p); MachineStepProgramFree(&steps);
    assert(!steps.moves && !steps.count); MachineStepProgramFree(&steps);
}
