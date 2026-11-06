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
