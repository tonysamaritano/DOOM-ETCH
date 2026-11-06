// SPDX-License-Identifier: GPL-2.0-or-later
// Only world placement, allocation and thinker scheduling are fixture adapters.
// All serialization runs through the game's production p_saveg.c.
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "p_local.h"
#include "p_saveg.h"
#include "doomstat.h"
#include "r_state.h"
#include "z_zone.h"

player_t players[MAXPLAYERS];
boolean playeringame[MAXPLAYERS];
state_t states[NUMSTATES];
mobjinfo_t mobjinfo[NUMMOBJTYPES];
sector_t *sectors;
line_t *lines;
side_t *sides;
int numsectors, numlines;
thinker_t thinkercap;
ceiling_t *activeceilings[MAXCEILINGS];
plat_t *activeplats[MAXPLATS];
static subsector_t placement;
static unsigned live;
void I_Error(char *format, ...)
{
    va_list args; va_start(args, format); vfprintf(stderr, format, args);
    va_end(args); abort();
}
void *Z_Malloc(int size, int tag, void *owner)
{
    assert(tag == PU_LEVEL && owner == NULL && size > 0);
    void *ptr = calloc(1, (size_t)size);
    assert(ptr); live++; return ptr;
}
void Z_Free(void *ptr) { assert(ptr && live); live--; free(ptr); }
void P_InitThinkers(void) { thinkercap.next = thinkercap.prev = &thinkercap; }
void P_AddThinker(thinker_t *th)
{
    th->next = &thinkercap; th->prev = thinkercap.prev;
    thinkercap.prev->next = th; thinkercap.prev = th;
}
void P_RemoveMobj(mobj_t *mo) { Z_Free(mo); }
void P_SetThingPosition(mobj_t *mo)
{
    placement.sector = &sectors[0]; mo->subsector = &placement;
    mo->snext = mo->sprev = mo->bnext = mo->bprev = NULL;
}
void P_MobjThinker(mobj_t *mo) { (void)mo; }
void T_MoveCeiling(ceiling_t *obj) { (void)obj; }
void T_VerticalDoor(vldoor_t *obj) { (void)obj; }
void T_MoveFloor(floormove_t *obj) { (void)obj; }
void T_PlatRaise(plat_t *obj) { (void)obj; }
void T_LightFlash(lightflash_t *obj) { (void)obj; }
void T_StrobeFlash(strobe_t *obj) { (void)obj; }
void T_Glow(glow_t *obj) { (void)obj; }
void P_AddActiveCeiling(ceiling_t *obj)
{
    for (int i = 0; i < MAXCEILINGS; i++)
        if (!activeceilings[i]) { activeceilings[i] = obj; return; }
    abort();
}
void P_AddActivePlat(plat_t *obj)
{
    for (int i = 0; i < MAXPLATS; i++)
        if (!activeplats[i]) { activeplats[i] = obj; return; }
    abort();
}
static void *Add(size_t size, actionf_p1 action)
{
    thinker_t *th = Z_Malloc((int)size, PU_LEVEL, NULL);
    th->function.acp1 = action; P_AddThinker(th); return th;
}
static void BuildWorld(sector_t *sec, line_t *line, side_t *side)
{
    sectors = sec; lines = line; sides = side; numsectors = 10; numlines = 1;
    for (int i = 0; i < numsectors; i++) {
        sec[i].floorheight = (-48 + i) * FRACUNIT;
        sec[i].ceilingheight = (96 + 3 * i) * FRACUNIT;
        sec[i].floorpic = 11 + i; sec[i].ceilingpic = 31 + i;
        sec[i].lightlevel = 200 - i; sec[i].tag = 70 + i;
    }
    line[0].sidenum[0] = 0; line[0].sidenum[1] = 1;
    line[0].flags = 9; line[0].special = 17; line[0].tag = 85;
    for (int i = 0; i < 2; i++) {
        side[i].textureoffset = (-7 + i) * FRACUNIT;
        side[i].rowoffset = (13 + i) * FRACUNIT;
        side[i].toptexture = 21 + i; side[i].midtexture = 41 + i;
    }
}
static void BuildPlayers(void)
{
    memset(players, 0, sizeof(players));
    memset(playeringame, 0, sizeof(playeringame));
    for (int i = 0; i < MAXPLAYERS; i += 2) {
        playeringame[i] = true; players[i].health = 81 - i;
        players[i].armorpoints = 27 + i; players[i].ammo[am_clip] = 34 + i;
        players[i].psprites[0].state = &states[7 + i];
    }
}
static void BuildObjects(void)
{
    for (int i = 0; i < MAXPLAYERS; i += 2) {
        mobj_t *mo = Add(sizeof(*mo), (actionf_p1)P_MobjThinker);
        mo->x = (18 + i) * FRACUNIT; mo->health = 81 - i;
        mo->state = &states[23 + i]; mo->type = MT_PLAYER;
        mo->player = &players[i]; players[i].mo = mo;
        mo->target = mo; mo->z = 6 * FRACUNIT;
    }
    ceiling_t *ceiling = Add(sizeof(*ceiling), (actionf_p1)T_MoveCeiling);
    ceiling->sector = &sectors[1]; ceiling->speed = 3 * FRACUNIT;
    vldoor_t *door = Add(sizeof(*door), (actionf_p1)T_VerticalDoor);
    door->sector = &sectors[2]; door->topwait = 37;
    floormove_t *floor = Add(sizeof(*floor), (actionf_p1)T_MoveFloor);
    floor->sector = &sectors[3]; floor->floordestheight = 29 * FRACUNIT;
    plat_t *plat = Add(sizeof(*plat), (actionf_p1)T_PlatRaise);
    plat->sector = &sectors[4]; plat->count = 19;
    lightflash_t *flash = Add(sizeof(*flash), (actionf_p1)T_LightFlash);
    flash->sector = &sectors[5]; flash->count = 43;
    strobe_t *strobe = Add(sizeof(*strobe), (actionf_p1)T_StrobeFlash);
    strobe->sector = &sectors[6]; strobe->brighttime = 11;
    glow_t *glow = Add(sizeof(*glow), (actionf_p1)T_Glow);
    glow->sector = &sectors[7]; glow->direction = -1;
    ceiling_t *stopped = Add(sizeof(*stopped), NULL);
    stopped->sector = &sectors[8]; stopped->olddirection = -1;
    P_AddActiveCeiling(stopped);
    plat_t *paused = Add(sizeof(*paused), NULL);
    paused->sector = &sectors[9]; paused->oldstatus = down;
    P_AddActivePlat(paused);
}
