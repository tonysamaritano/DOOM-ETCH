#include "doomtype.h"
#include "z_zone.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static byte *arena;
byte *I_ZoneBase(int *size)
{
    *size = 65536;
    arena = malloc((size_t)*size);
    assert(arena);
    return arena;
}
void I_Error(char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}
int main(void)
{
    Z_Init();
    int empty = Z_FreeMemory();
    void *items[24];
    for (int i = 0; i < 24; ++i) {
        items[i] = Z_Malloc(7 + i, PU_STATIC, NULL);
        memset(items[i], i + 1, 7 + i);
    }
    for (int i = 0; i < 24; i += 2) Z_Free(items[i]);
    Z_CheckHeap();
    for (int i = 1; i < 24; i += 2) {
