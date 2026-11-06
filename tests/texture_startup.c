// SPDX-License-Identifier: GPL-2.0-or-later
// Include the real renderer so its private disk record is checked as well.
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include "../engine/render/r_data.c"

boolean demoplayback;
int numsectors, numsides, numsprites, skytexture;
sector_t *sectors;
side_t *sides;
spritedef_t *sprites;
thinker_t thinkercap;
lumpinfo_t *lumpinfo;
void P_MobjThinker(mobj_t *object) { (void)object; }
void Z_ChangeTag2(void *ptr, int tag) { (void)ptr; (void)tag; }
static const char *mode;
static unsigned allocationCount;
static void *allocated[64];
static uintptr_t colormapAllocation;
void I_Error(char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    abort();
}
