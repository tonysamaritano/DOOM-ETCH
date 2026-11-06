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

static void modal_program(void)
{
    MachineProgram p = {0}; MachineParseError error = {0};
    MachineParseOptions options = {{1, 2}};
    const char *text = "; heading\r\nG21 G90\r\nG94 G61\r\nF3600\r\n"
        "G1 X3 (first)\r\nY1 ; modal\r\nM0\r\nG0 X1\r\nM2";
    assert(MachineGCodeParse(&p, text, &options, &error));
    assert(p.count == 5);
    machine_point(p.start_cnc_mm, 1, 2);
    machine_point(p.moves[0].cnc_mm, 3, 2);
    machine_point(p.moves[1].cnc_mm, 3, 1);
    machine_point(p.moves[2].cnc_mm, 3, 1);
    machine_point(p.moves[3].cnc_mm, 1, 1);
    machine_point(p.moves[4].cnc_mm, 1, 1);
    for (size_t i = 0; i < p.count; ++i) assert(p.moves[i].line == i + 5);
    assert(p.moves[0].kind == MACHINE_MOVE_DRAW);
    assert(p.moves[2].kind == MACHINE_MOVE_PAUSE);
    assert(p.moves[3].kind == MACHINE_MOVE_RAPID && p.moves[3].feed_mm_s == 0);
    assert(p.moves[4].kind == MACHINE_MOVE_END);
    machine_near(p.moves[0].feed_mm_s, 60);
    MachineProgramFree(&p);
}
