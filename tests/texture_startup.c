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
static unsigned char textureBytes[78];
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
int W_LumpLength(int lump) { return lump == 2 ? sizeof(textureBytes) : 256; }
void *W_CacheLumpName(char *name, int tag)
{
    (void)tag;
    const void *source = !strcmp(name, "PNAMES") ? (const void *)namesBytes : textureBytes;
    size_t length = !strcmp(name, "PNAMES") ? sizeof(namesBytes) : sizeof(textureBytes);
    void *copy = malloc(length);
    assert(copy);
    memcpy(copy, source, length);
    return copy;
}
void *W_CacheLumpNum(int lump, int tag)
{
    (void)tag;
    assert(lump == 1);
    return (void *)patchBytes;
}
void W_ReadLump(int lump, void *dest)
{
    assert(lump == 5);
    uintptr_t address = (uintptr_t)dest;
    fprintf(stderr, "colormap allocation=%p aligned=%p\n", (void *)colormapAllocation, dest);
    assert((address & 255) == 0);
    assert(address >= colormapAllocation && address - colormapAllocation <= 255);
    memset(dest, 42, 256);
}
static void Put16(unsigned char *p, unsigned value)
{
    p[0] = value & 255;
    p[1] = (value >> 8) & 255;
}
static void Put32(unsigned char *p, unsigned value)
{
    Put16(p, value);
    Put16(p + 2, value >> 16);
}
static void MakeTextures(void)
{
    Put32(textureBytes, 2);
    for (unsigned i = 0; i < 2; i++) {
        // WAD records can start on a two-byte boundary, as in shareware E1M1.
        unsigned offset = 14 + i * 32;
        Put32(textureBytes + 4 + i * 4, offset);
        unsigned char *record = textureBytes + offset;
        memcpy(record, i ? "SECOND" : "FIRST", i ? 6 : 5);
        Put16(record + 12, 2); // width after the 32-bit masked flag
        Put16(record + 14, 1);
        Put32(record + 16, 0x12345678); // obsolete field is a disk integer
        Put16(record + 20, 1); // patch count
        Put16(record + 22, 0); // x
        Put16(record + 24, 0); // y
        Put16(record + 26, 0); // patch index
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    mode = argv[1];
    if (!strcmp(mode, "colormap")) {
        R_InitColormaps();
        assert(colormaps[0] == 42 && colormaps[255] == 42);
    } else {
        if (!strcmp(mode, "record")) {
            fprintf(stderr, "disk patches offset=%zu; expected 22\n", offsetof(maptexture_t, patches));
            assert(offsetof(maptexture_t, patches) == 22);
        }
        MakeTextures();
        R_InitTextures();
        assert(numtextures == 2 && textureheight[1] == FRACUNIT);
        assert(R_TextureNumForName("SECOND") == 1);
        assert(R_GetColumn(1, 1)[0] == 7);
    }
    for (unsigned i = 0; i < allocationCount; i++) free(allocated[i]);
    return 0;
}
