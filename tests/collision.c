#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#define close doom_door_close
#include "p_local.h"
#undef close
#include "r_state.h"
#include "m_bbox.h"
#define MM(n) ((n) * FRACUNIT)

int bmapwidth = 4, bmapheight = 4, validcount;
fixed_t bmaporgx = -MM(256), bmaporgy = -MM(256);
short *blockmap, *blockmaplump;
mobj_t **blocklinks;
line_t *lines;
int numlines;
static sector_t rooms[2];
static subsector_t areas[2];
static line_t walls[MAXINTERCEPTS + 1];
static vertex_t endpoints[2 * (MAXINTERCEPTS + 1)];
static short cells[16 + MAXINTERCEPTS + 2];
static mobj_t *links[16], body, crowd[MAXINTERCEPTS + 1];
subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{ (void)y; return &areas[x > 0]; }
void I_Error(char *format, ...)
{
    va_list args; va_start(args, format);
    vfprintf(stderr, format, args); va_end(args);
    exit(1);
}
void P_DamageMobj(mobj_t *a, mobj_t *b, mobj_t *c, int damage)
