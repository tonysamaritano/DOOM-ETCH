#include "gcode.h"
#include "machine_test.h"

static const char *invalid[] = {
    "G2 X1", "G20", "G91", "G92 X0", "M3", "G1 Z1 F60",
    "G1 X1 X2 F60", "G1 G0 X1 F60", "G1 X1", "X1 F60",
    "F0", "F-1", "FNaN", "FInf", "G1 Xnan F60", "G1 Xinf F60",
    "G1 X1e2 F60", "G1 X. F60", "G1 X1foo F60", "G1 X1 F60 junk",
    "(nested (comment))", "(unclosed", "G1 X1 F60)", "M2\nG1 X1 F60"
};
