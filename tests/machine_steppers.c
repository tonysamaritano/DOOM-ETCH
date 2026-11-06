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

static void compile_rejection(void)
{
    MachineSettings s; MachineSettingsDefault(&s); MachineParseError e = {0};
    MachineProgram p = {0}; MachineStepProgram out = {0};
    assert(MachineGCodeParse(&p, "G1 X1 Y1 F60", NULL, &e));
    assert(MachineSteppersCompile(&out, &p, &s, &e));
    MachineStep *owned = out.moves;
    p.moves[0].cnc_mm.x = s.width_mm + 1;
    assert(!MachineSteppersCompile(&out, &p, &s, &e));
    assert(e.line == 1 && e.message[0] && out.moves == owned);
    assert(out.count == 1 && out.total_steps[0] == 20);
    p.moves[0].cnc_mm.x = NAN;
    assert(!MachineSteppersCompile(&out, &p, &s, &e));
    assert(out.moves == owned && out.count == 1);
    p.moves[0].cnc_mm.x = 1;
    s.steps_per_mm[0] = 1e300;
    assert(!MachineSteppersCompile(&out, &p, &s, &e));
    assert(out.moves == owned);
    MachineProgramFree(&p); MachineStepProgramFree(&out);
}

static void device_grid_phase(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    s.width_mm = 1.023; s.height_mm = 1.037;
    MachineParseOptions options = {{0, 1.037}};
    MachineProgram p = {0}; MachineStepProgram out = {0}; MachineParseError e = {0};
    const char *text = "F60\nG1 X0.025 Y1.012\nX0.024999999 Y1.012000001\n"
        "X0.025000001 Y1.011999999\nX1.023 Y0\nM2";
    assert(MachineGCodeParse(&p, text, &options, &e));
    assert(MachineSteppersCompile(&out, &p, &s, &e));
    assert(out.start_steps[0] == 0 && out.start_steps[1] == 0);
    assert(out.moves[0].target_steps[0] == 1 && out.moves[0].target_steps[1] == 1);
    assert(out.moves[1].target_steps[0] == 0 && out.moves[1].target_steps[1] == 0);
    assert(out.moves[2].target_steps[0] == 1 && out.moves[2].target_steps[1] == 1);
    assert(out.moves[3].target_steps[0] == 20 && out.moves[3].target_steps[1] == 20);
    MachineProgramFree(&p); MachineStepProgramFree(&out);
}

int main(void)
{
    counters(); compile_rejection(); device_grid_phase();
    puts("machine-steppers: device grid, counts and transactional compilation passed");
    return 0;
}
