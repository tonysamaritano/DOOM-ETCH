// SPDX-License-Identifier: GPL-2.0-or-later
#include "gcode.h"
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static int fail(MachineParseError *e,size_t line,const char *message)
{
    if (e) { e->line=line; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
void MachineProgramFree(MachineProgram *p)
{
    if (p) { free(p->moves); *p=(MachineProgram){0}; }
}
static int append(MachineProgram *p,MachineMove move)
{
    if (p->count==p->capacity) {
        size_t capacity=p->capacity ? p->capacity*2 : 32;
        if (capacity<p->capacity || capacity>SIZE_MAX/sizeof(*p->moves)) return 0;
        MachineMove *next=realloc(p->moves,capacity*sizeof(*next));
        if (!next) return 0;
        p->moves=next; p->capacity=capacity;
    }
    p->moves[p->count++]=move;
    return 1;
}
typedef struct {
    MachineProgram program;
    MotionPoint cursor;
    double feed;
    int mode,ended;
} Parser;
