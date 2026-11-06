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
    for (int i = 0; i < 16; ++i) cells[i] = 16;
    blockmap = blockmaplump = cells;
    blocklinks = links;
    lines = walls; numlines = count;
    body.x = -MM(64); body.radius = MM(16); body.height = MM(56);
    body.ceilingz = MM(128); body.flags = MF_SOLID;
    P_SetThingPosition(&body);
}
static void moveBoundaries(void)
{
    scene(1, 0);
    walls[0].backsector = NULL;
    walls[0].flags = 0;
    assert(!P_TryMove(&body, -MM(8), 0));
    assert(body.x == -MM(64));
    assert(P_TryMove(&body, -MM(16), 0));
    assert(!P_TryMove(&body, -MM(16) + 1, 0));
    body.flags |= MF_NOCLIP;
    assert(P_TryMove(&body, MM(8), 0));
    assert(body.subsector == &areas[1] && rooms[0].thinglist == NULL);
    for (int height = 55; height <= 57; ++height) {
        scene(1, 0); rooms[1].ceilingheight = MM(height);
        assert(P_TryMove(&body, MM(8), 0) == (height >= 56));
    }
    for (int step = 23; step <= 25; ++step) {
        scene(1, 0); rooms[1].floorheight = MM(step);
        body.flags |= MF_DROPOFF;
        assert(P_TryMove(&body, MM(8), 0) == (step <= 24));
    }
    scene(1, 0); rooms[1].floorheight = MM(24) + 1;
    body.flags |= MF_DROPOFF;
    assert(!P_TryMove(&body, MM(8), 0));
    scene(1, 0); walls[0].flags |= ML_BLOCKING;
    assert(!P_TryMove(&body, MM(8), 0));
    scene(0, 0);
    crowd[0] = (mobj_t){.x = MM(24), .radius = MM(16), .flags = MF_SOLID};
    P_SetThingPosition(&crowd[0]);
    assert(P_TryMove(&body, -MM(8), 0));
    assert(!P_TryMove(&body, -MM(8) + 1, 0));
    P_UnsetThingPosition(&crowd[0]);
    assert(P_TryMove(&body, 0, MM(64)));
    P_UnsetThingPosition(&body);
    for (int i = 0; i < 16; ++i) assert(links[i] == NULL);
}
static int hits;
static fixed_t previousFraction;
static boolean hit(intercept_t *entry)
{
    assert(entry->frac >= previousFraction && entry->frac <= FRACUNIT);
    previousFraction = entry->frac; ++hits; return true;
}
static void traverse(int count, int things)
{
    scene(things ? 0 : count, 0);
    if (things) {
        P_UnsetThingPosition(&body);
        memset(crowd, 0, sizeof(crowd));
        for (int i = 0; i < count; ++i) {
            crowd[i].x = -MM(4) + i * 1024;
            crowd[i].y = MM(8); crowd[i].radius = MM(1);
            P_SetThingPosition(&crowd[i]);
        }
    }
    hits = 0; previousFraction = 0;
    assert(P_PathTraverse(-MM(64), MM(8), MM(64), MM(8),
                          things ? PT_ADDTHINGS : PT_ADDLINES, hit));
    assert(hits == count);
}
static void overflowSpecials(void)
{ scene(9, 1); (void)P_CheckPosition(&body, 0, MM(8)); }
static void overflowLines(void) { traverse(MAXINTERCEPTS + 1, 0); }
static void overflowThings(void) { traverse(MAXINTERCEPTS + 1, 1); }
static void expectFailure(void (*operation)(void), const char *message)
{
    int channel[2]; assert(pipe(channel) == 0);
    pid_t child = fork(); assert(child >= 0);
    if (child == 0) {
        close(channel[0]); dup2(channel[1], STDERR_FILENO); close(channel[1]);
        operation(); _exit(99);
    }
    close(channel[1]);
    char text[8192]; size_t used = 0; ssize_t n;
    while ((n = read(channel[0], text + used, sizeof(text) - used - 1)) > 0)
        used += (size_t)n;
    text[used] = 0; close(channel[0]);
    int status; assert(waitpid(child, &status, 0) == child);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 1 || !strstr(text, message))
        fprintf(stderr, "Expected controlled engine error %s; status=%d; got: %s\n", message, status, text);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    assert(strstr(text, message));
    assert(!strstr(text, "AddressSanitizer") && !strstr(text, "runtime error"));
}
int main(void)
{
    moveBoundaries();
    scene(8, 1); assert(P_CheckPosition(&body, 0, MM(8)));
    expectFailure(overflowSpecials, "MAXSPECIALCROSS exceeded");
    traverse(MAXINTERCEPTS, 0);
    expectFailure(overflowLines, "MAXINTERCEPTS exceeded");
    traverse(MAXINTERCEPTS, 1);
    expectFailure(overflowThings, "MAXINTERCEPTS exceeded");
    return 0;
}
