/* Device-free Doom services for rendering a game view from the command line. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i_sound.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "w_wad.h"

void I_Error(char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

byte *I_AllocLow(int bytes)
{
    byte *memory = calloc((size_t)bytes, 1);
    if (!memory)
        I_Error("Unable to allocate %d bytes for headless rendering", bytes);
    return memory;
}

byte *I_ZoneBase(int *bytes)
{
    *bytes = 32 * 1024 * 1024;
    return I_AllocLow(*bytes);
}
