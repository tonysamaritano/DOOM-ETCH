// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdio.h>
#include <string.h>
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

int WriteMotionOutput(const char *prefix, const char *extension, const MotionPath *path);

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "--no-sprites"))) {
        fprintf(stderr, "Usage: %s IWAD PREFIX [--no-sprites]\n", argv[0]);
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
    R_EtchSprites(argc == 3);
    if (!R_EtchBegin(viewwidth, viewheight))
        I_Error("Cannot capture the E1M1 view");
    R_RenderPlayerView(&players[0]);
    MotionPath path = {0};
    if (!R_EtchBuildMotion(&path)) I_Error("Cannot plan the captured walls");
    if (!WriteMotionOutput(argv[2], "svg", &path)) I_Error("Cannot write %s.svg", argv[2]);
    if (!WriteMotionOutput(argv[2], "gcode", &path)) I_Error("Cannot write %s.gcode", argv[2]);
    printf("%zu moves at %.2f x %.2f mm (assumed feed 3600 mm/min)\n",
           path.count - 1, path.width_mm, path.height_mm);
    MotionFree(&path);
    R_EtchResetSprites();
    return 0;
}
