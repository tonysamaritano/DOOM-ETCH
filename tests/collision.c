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
{ (void)a; (void)b; (void)c; (void)damage; abort(); }
boolean P_SetMobjState(mobj_t *thing, statenum_t state)
{ (void)thing; (void)state; abort(); }
void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher)
{ (void)special; (void)toucher; abort(); }
void P_CrossSpecialLine(int line, int side, mobj_t *thing)
{ (void)line; (void)side; (void)thing; }
static void scene(int count, int special)
{
    memset(rooms, 0, sizeof(rooms));
    memset(links, 0, sizeof(links));
    memset(&body, 0, sizeof(body));
    memset(walls, 0, sizeof(walls));
    for (int i = 0; i < 2; ++i) {
        rooms[i].ceilingheight = MM(128);
        areas[i].sector = &rooms[i];
    }
    for (int i = 0; i < count; ++i) {
        fixed_t x = count == 1 ? 0 : -MM(4) + i * 1024;
        endpoints[i * 2] = (vertex_t){x, -MM(128)};
        endpoints[i * 2 + 1] = (vertex_t){x, MM(128)};
        walls[i].v1 = &endpoints[i * 2];
        walls[i].v2 = &endpoints[i * 2 + 1];
        walls[i].dy = MM(256);
        walls[i].slopetype = ST_VERTICAL;
        walls[i].bbox[BOXLEFT] = walls[i].bbox[BOXRIGHT] = x;
        walls[i].bbox[BOXBOTTOM] = -MM(128);
        walls[i].bbox[BOXTOP] = MM(128);
        walls[i].frontsector = &rooms[0];
        walls[i].backsector = &rooms[1];
        walls[i].flags = ML_TWOSIDED;
        walls[i].special = special;
        cells[16 + i] = i;
    }
    cells[16 + count] = -1;
