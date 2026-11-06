#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "i_system.h"
#include "doomdef.h"

int I_GetTime(void)
{
    static uint64_t origin;
    uint64_t now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    if (!origin)
        origin = now;
    uint64_t elapsed = now - origin;
    return (int)((elapsed / 1000000000) * TICRATE
                 + (elapsed % 1000000000) * TICRATE / 1000000000);
}

ticcmd_t *I_BaseTiccmd(void)
{
    static ticcmd_t empty;
    return &empty;
}

byte *I_ZoneBase(int *size)
{
    *size = 32 * 1024 * 1024;
    byte *zone = malloc((size_t)*size);
    if (!zone)
        I_Error("Could not allocate the %d-byte Doom zone", *size);
    return zone;
}

byte *I_AllocLow(int length)
{
    byte *memory = calloc((size_t)length, 1);
    if (!memory)
        I_Error("Could not allocate %d bytes", length);
    return memory;
}

/* The optional DOS tactile device has no equivalent on this platform. */
void I_Tactile(int on, int off, int total)
{
    (void)on;
    (void)off;
    (void)total;
}
