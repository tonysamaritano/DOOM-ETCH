#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "i_system.h"
#include "w_wad.h"
#include "z_zone.h"

enum { REJECTED = 41 };

void I_Error(char *message, ...)
{
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    _exit(REJECTED);
}

byte *I_ZoneBase(int *size)
{
    static _Alignas(memblock_t) byte arena[8192];
    *size = sizeof(arena);
    return arena;
}
