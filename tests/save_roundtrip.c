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
