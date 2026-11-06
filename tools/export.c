// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "export_files.h"
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


static void ReportMotion(const MotionPath *path)
{
    printf("%zu moves at %.2f x %.2f mm (assumed feed 3600 mm/min)\n",
           path->count - 1, path->width_mm, path->height_mm);
    double length_mm = 0;
    for (size_t i = 1; i < path->count; ++i) {
        double dx = (path->points[i].x - path->points[i - 1].x) * path->width_mm / path->width;
        double dy = (path->points[i].y - path->points[i - 1].y) * path->height_mm / path->height;
        length_mm += hypot(dx, dy);
    }
    /* Ideal constant-speed estimate, with the feed assumed to be 60 mm/s. */
    printf("PATH length_mm=%.9f estimate_seconds=%.9f\n", length_mm, length_mm / 60);
    puts("Estimate assumes constant 60 mm/s, excluding acceleration and backlash.");
}

static int ParseFrames(const char *text, unsigned *frames)
{
    unsigned value = 0;
    if (!*text) return 0;
    for (; *text; text++) {
        if (*text < '0' || *text > '9' || value > 1000) return 0;
        value = value * 10 + (unsigned)(*text - '0');
    }
    // Bound batch size to the reference's supported 1..10000 frames.
    if (value < 1 || value > 10000) return 0;
    *frames = value;
    return 1;
}

int main(int argc, char **argv)
{
    unsigned frames = 1;
    if (argc < 3) {
        fprintf(stderr, "Usage: %s IWAD PREFIX [--no-sprites] [--no-hud] [--no-weapon]\n"
            "  [--no-textures] [--no-sprite-textures] [--no-weapon-textures] [--hatch-pattern 0..2] [--frames 1..10000]\n", argv[0]);
        return 2;
    }
    for (int argument = 3; argument < argc; ++argument) {
        if (!strcmp(argv[argument], "--no-sprites")) R_EtchSprites(0);
        else if (!strcmp(argv[argument], "--no-hud")) R_EtchHUD(0);
        else if (!strcmp(argv[argument], "--no-weapon")) R_EtchWeapon(0);
        else if (!strcmp(argv[argument], "--no-textures")) R_EtchTextures(0);
        else if (!strcmp(argv[argument], "--no-sprite-textures")) R_EtchSpriteTextures(0);
        else if (!strcmp(argv[argument], "--no-weapon-textures")) R_EtchWeaponTextures(0);
        else if (!strcmp(argv[argument], "--frames")) {
            if (argument + 1 >= argc || !ParseFrames(argv[++argument], &frames)) {
                fputs("Frames must be a decimal integer in 1..10000\n", stderr);
                return 2;
            }
        }
        else if (!strcmp(argv[argument], "--hatch-pattern") && argument + 1 < argc) {
            const char *pattern = argv[++argument];
            if (pattern[0] < '0' || pattern[0] > '2' || pattern[1]) {
                fprintf(stderr, "Hatch pattern must be 0, 1 or 2\n");
                return 2;
            }
            R_EtchHatchPattern(pattern[0] - '0');
        }
        else {
            fprintf(stderr, "Unknown export option: %s\n", argv[argument]);
            return 2;
        }
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
    R_SetViewSize(R_EtchHUDEnabled() ? 10 : 11, 0);
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
    ST_EtchDrawer();
    MotionPath path = {0};
    if (!R_EtchBuildMotion(&path)) I_Error("Cannot plan the captured frame");
    if (!WriteMotionOutput(argv[2], "svg", &path)) I_Error("Cannot write %s.svg", argv[2]);
    if (!WriteMotionOutput(argv[2], "gcode", &path)) I_Error("Cannot write %s.gcode", argv[2]);
    ReportMotion(&path);
    MotionFree(&path);
    R_EtchResetSprites();
    return 0;
}
