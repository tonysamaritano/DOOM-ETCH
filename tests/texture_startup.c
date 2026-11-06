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
void *Z_Malloc(int size, int tag, void *owner)
{
    (void)tag;
    if (!strcmp(mode, "tables") && allocationCount < 4) {
        fprintf(stderr, "pointer table %u requests %d bytes; needs %zu\n",
            allocationCount, size, 2 * sizeof(void *));
        assert(size == 2 * sizeof(void *));
    }
    assert(size > 0 && allocationCount < 64);
    void *ptr = calloc(1, (size_t)size);
    assert(ptr);
    allocated[allocationCount++] = ptr;
    if (owner) *(void **)owner = ptr;
    if (!strcmp(mode, "colormap")) colormapAllocation = (uintptr_t)ptr;
    return ptr;
}
void Z_Free(void *ptr)
{
    for (unsigned i = 0; i < allocationCount; i++)
        if (allocated[i] == ptr) allocated[i] = NULL;
    free(ptr);
}
static const unsigned char patchBytes[] = {
    2,0, 1,0, 0,0, 0,0, 16,0,0,0, 16,0,0,0, 0,1,0,7,0,255
};
static unsigned char textureBytes[76];
static const unsigned char namesBytes[] = {1,0,0,0,'P','A','T','C','H',0,0,0};
int W_CheckNumForName(char *name)
{
    const char *names[] = {"PNAMES", "PATCH", "TEXTURE1", "S_START", "S_END", "COLORMAP"};
    for (int i = 0; i < 6; i++) if (!strcmp(name, names[i])) return i;
    return -1;
}
int W_GetNumForName(char *name)
{
    int result = W_CheckNumForName(name);
    assert(result >= 0);
    return result;
}
