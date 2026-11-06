#include "gcode.h"
#include "machine_test.h"

static const char *invalid[] = {
    "G2 X1", "G20", "G91", "G92 X0", "M3", "G1 Z1 F60",
    "G1 X1 X2 F60", "G1 G0 X1 F60", "G1 X1", "X1 F60",
    "F0", "F-1", "FNaN", "FInf", "G1 Xnan F60", "G1 Xinf F60",
    "G1 X1e2 F60", "G1 X. F60", "G1 X1foo F60", "G1 X1 F60 junk",
    "(nested (comment))", "(unclosed", "G1 X1 F60)", "M2\nG1 X1 F60"
};

static void rejected_programs(void)
{
    MachineProgram p = {0}; MachineParseError error = {0};
    assert(MachineGCodeParse(&p, "G1 X2 Y3 F3600", NULL, &error));
    MachineMove *owned = p.moves;
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        char text[256];
        snprintf(text, sizeof text, "; first physical line\n\n%s", invalid[i]);
        assert(!MachineGCodeParse(&p, text, NULL, &error));
        assert(error.line >= 3 && error.message[0]);
        assert(p.moves == owned && p.count == 1);
        machine_point(p.moves[0].cnc_mm, 2, 3);
        machine_near(p.moves[0].feed_mm_s, 60);
    }
    MachineProgramFree(&p);
    assert(!p.moves && !p.count);
    MachineProgramFree(&p);
}
