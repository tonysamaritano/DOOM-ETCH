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

int I_GetTime(void)
{
    static int tick;
    return tick++;
}

ticcmd_t *I_BaseTiccmd(void)
{
    static ticcmd_t idle;
    return &idle;
}

void I_Init(void) {}
void I_Quit(void) { exit(EXIT_SUCCESS); }
void I_StartFrame(void) {}
void I_StartTic(void) {}
void I_InitGraphics(void) {}
void I_ShutdownGraphics(void) {}
void I_SetPalette(byte *palette) { (void)palette; }
void I_UpdateNoBlit(void) {}
void I_FinishUpdate(void) {}
void I_WaitVBL(int count) { (void)count; }
void I_BeginRead(void) {}
void I_EndRead(void) {}
void I_ReadScreen(byte *dest)
{
    memcpy(dest, screens[0], SCREENWIDTH * SCREENHEIGHT);
}
void I_Tactile(int on, int off, int total)
{
    (void)on; (void)off; (void)total;
}
