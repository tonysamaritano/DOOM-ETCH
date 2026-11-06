/* Exercise the renderer's table builder with an ordinary 64-bit allocation. */
#include "doomtype.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

void R_InitTranslationTables(void);
extern byte *translationtables;
static byte *allocation;
static size_t allocationSize;
void *Z_Malloc(int size, int tag, void *owner)
{
    (void)tag;
    assert(owner == NULL);
    allocationSize = (size_t)size;
    allocation = malloc(allocationSize);
    assert(allocation);
    return allocation;
}
int main(void)
{
    R_InitTranslationTables();
    uintptr_t start = (uintptr_t)allocation;
    uintptr_t table = (uintptr_t)translationtables;
    assert(table >= start && table + 768 <= start + allocationSize);
    assert((table & 255) == 0);
    for (int ramp = 0; ramp < 3; ++ramp)
        for (int color = 0; color < 256; ++color) {
            int expected = color;
            if (color >= 0x70 && color <= 0x7f)
                expected = 0x60 - ramp * 0x20 + (color & 15);
            assert(translationtables[ramp * 256 + color] == expected);
        }
    free(allocation);
    return 0;
}
