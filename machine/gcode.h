// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_MACHINE_GCODE_H
#define DOOM_MACHINE_GCODE_H
#include "motion.h"
#include <stdio.h>
typedef enum {
    MACHINE_MOVE_DRAW, MACHINE_MOVE_RAPID, MACHINE_MOVE_PAUSE, MACHINE_MOVE_END
} MachineMoveKind;
typedef struct {
    MotionPoint cnc_mm; /* +Y up, millimetres. */
    double feed_mm_s; /* G0 uses zero: hardware axis limits only. */
    MachineMoveKind kind;
    size_t line; /* One-based physical source line. */
} MachineMove;
typedef struct {
    MachineMove *moves;
    size_t count, capacity;
    MotionPoint start_cnc_mm;
} MachineProgram;
typedef struct { MotionPoint start_cnc_mm; } MachineParseOptions;
typedef struct { size_t line; char message[160]; } MachineParseError;
/* Zero-initialize owned programs; failures preserve them. Streams stay open.
 * Maximum physical line length is 80 bytes excluding LF/CRLF. */
int MachineGCodeParse(MachineProgram *, const char *, const MachineParseOptions *, MachineParseError *);
int MachineGCodeParseFile(MachineProgram *, FILE *, const MachineParseOptions *, MachineParseError *);
void MachineProgramFree(MachineProgram *);
#endif
