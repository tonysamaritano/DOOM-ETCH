#include <stdint.h>
#include <stdlib.h>
#include "i_system.h"
#include "z_zone.h"

byte *I_ZoneBase(int *size)
{
    static _Alignas(memblock_t) byte memory[4096];
    *size = sizeof(memory);
    return memory;
}

void I_Error(char *message, ...)
{
    fprintf(stderr, "%s\n", message);
    abort();
}

int main(void)
{
    Z_Init();
    Z_Malloc(1, PU_STATIC, NULL);
    void *next = Z_Malloc(sizeof(void *), PU_STATIC, NULL);
    if ((uintptr_t)next % _Alignof(void *)) {
        fputs("An odd-sized lump misaligned the next zone allocation\n", stderr);
        return EXIT_FAILURE;
    }
    Z_CheckHeap();
    return EXIT_SUCCESS;
}
