// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdio.h>
#include "doomstat.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_menu.h"
#include "p_local.h"
#include "r_local.h"
#include "r_etch.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

void D_CheckNetGame(void);
void R_ExecuteSetViewSize(void);

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s IWAD walls.csv\n", argv[0]);
        return 2;
    }
    myargc = argc; myargv = argv;
    char *files[] = {argv[1], NULL};
    V_Init();
    Z_Init();
    W_InitMultipleFiles(files);
    if (W_CheckNumForName("E1M1") < 0 || W_CheckNumForName("MAP01") >= 0)
        I_Error("Capture requires the shareware doom1.wad");
    gamemode = shareware;
    playeringame[0] = true;
    consoleplayer = displayplayer = 0;
