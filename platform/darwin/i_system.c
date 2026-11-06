#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "i_system.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_net.h"
#include "g_game.h"
#include "i_sound.h"
#include "i_video.h"
#include "m_misc.h"
#include "metal_backend.h"

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

void I_Init(void)
{
    I_InitSound();
    I_InitMusic();
}

void I_Quit(void)
{
    if (demorecording)
        G_CheckDemoStatus();
    D_QuitNetGame();
    I_ShutdownSound();
    I_ShutdownMusic();
    M_SaveDefaults();
    fprintf(stderr, "Presented %u frames\n", I_MetalPresentedFrames());
    I_ShutdownGraphics();
    exit(EXIT_SUCCESS);
}
