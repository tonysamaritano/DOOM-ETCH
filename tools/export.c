// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "doomstat.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_menu.h"
#include "p_local.h"
#include "p_setup.h"
#include "r_local.h"
#include "r_etch.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

void D_CheckNetGame(void);
void R_ExecuteSetViewSize(void);

/* Publish each completed file by rename, so a failed write preserves its predecessor. */
static int write_output(const char *prefix, const char *extension, const MotionPath *path)
{
    char filename[PATH_MAX], temporary[PATH_MAX];
    int n = snprintf(filename, sizeof(filename), "%s.%s", prefix, extension);
    if (n < 0 || (size_t)n >= sizeof(filename)) return 0;
    n = snprintf(temporary, sizeof(temporary), "%s.XXXXXX", filename);
    if (n < 0 || (size_t)n >= sizeof(temporary)) return 0;

    int fd = mkstemp(temporary);
    if (fd < 0) return 0;
    FILE *stream = fdopen(fd, "w");
    int success = 0;
    if (stream) {
        /* Assumed feed: 3,600 mm/min for the simulated machine. */
        success = extension[0] == 's' ? MotionWriteSVG(stream, path)
                                     : MotionWriteGCode(stream, path, 3600.0);
        if (fclose(stream)) success = 0;
    } else {
        close(fd);
    }
    if (success && rename(temporary, filename) == 0) return 1;
    unlink(temporary);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s IWAD PREFIX\n", argv[0]);
        return 2;
    }
    myargc = argc; myargv = argv;
    char *files[] = {argv[1], NULL};
    V_Init();
    Z_Init();
    W_InitMultipleFiles(files);
    if (W_CheckNumForName("E1M1") < 0 || W_CheckNumForName("E2M1") >= 0
        || W_CheckNumForName("MAP01") >= 0)
        I_Error("Export requires the shareware doom1.wad");
    gamemode = shareware;
    playeringame[0] = true;
    consoleplayer = displayplayer = 0;
    M_Init();
    R_Init();
    R_SetViewSize(11, 0);
    R_ExecuteSetViewSize();
    P_Init();
    S_Init(8, 8);
    HU_Init();
    ST_Init();
    D_CheckNetGame();
    singletics = true;
    G_InitNew(sk_medium, 1, 1);
    for (int tic = 0; tic < 20; ++tic) {
        G_Ticker();
        ++gametic;
    }
    if (!R_EtchBegin(viewwidth, viewheight))
        I_Error("Cannot capture the E1M1 view");
    R_RenderPlayerView(&players[0]);
    MotionPath path = {0};
    if (!R_EtchBuildMotion(&path)) I_Error("Cannot plan the captured walls");
    if (!write_output(argv[2], "svg", &path)) I_Error("Cannot write %s.svg", argv[2]);
    if (!write_output(argv[2], "gcode", &path)) I_Error("Cannot write %s.gcode", argv[2]);
    printf("%zu moves at %.2f x %.2f mm (assumed feed 3600 mm/min)\n",
           path.count - 1, path.width_mm, path.height_mm);
    MotionFree(&path);
    return 0;
}
