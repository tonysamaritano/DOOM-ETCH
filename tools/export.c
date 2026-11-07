// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "export_files.h"
#include "bridge.h"
#include "requirements.h"
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

static int CommandRequirements(FILE *stream,double fps,MachineRequirementsFrames *out)
{
    MachineSettings settings; MachineSettingsDefault(&settings);
    MachineProgram parsed={0}; MachineStepProgram compiled={0}; MachineParseError error={0};
    int ok=fflush(stream)==0 && !ferror(stream) && fseek(stream,0,SEEK_SET)==0;
    if (ok) ok=MachineRead(stream,&settings,&parsed,&error);
    if (ok) ok=MachineSteppersCompile(&compiled,&parsed,&settings,&error);
    if (ok) ok=MachineRequirementsFramesBuild(out,&compiled,fps,&error);
    MachineProgramFree(&parsed); MachineStepProgramFree(&compiled);
    if (!ok) fprintf(stderr,"Command requirements at line %zu: %s\n",error.line,
        error.message[0] ? error.message : "cannot read completed temporary G-code");
    return ok;
}

static int CaptureFrame(MotionPath *path, MotionPoint cursor_mm)
{
    if (!R_EtchBegin(viewwidth, viewheight)) return 0;
    R_RenderPlayerView(&players[0]);
    ST_EtchDrawer();
    EtchSnapshot snapshot = {0};
    if (!R_EtchTakeSnapshot(&snapshot)) return 0;
    MotionPoint start = {cursor_mm.x / MOTION_WIDTH_MM * snapshot.width,
        cursor_mm.y / MOTION_HEIGHT_MM * snapshot.height};
    int success = MotionPlanSegments(path, snapshot.segments, snapshot.count,
        snapshot.width, snapshot.height, start);
    free(snapshot.segments);
    return success;
}

static int WriteFrameSVG(const char *prefix, unsigned frame, const MotionPath *path)
{
    if (!frame) return WriteMotionOutput(prefix, "svg", path);
    char filename[PATH_MAX];
    int length = snprintf(filename, sizeof(filename), "%s-frame%03u", prefix, frame);
    if (length < 0 || (size_t)length >= sizeof(filename)) return 0;
    return WriteMotionOutput(filename, "svg", path);
}

static int ExportFrames(const char *prefix, unsigned frames,double target_fps)
{
    MotionOutput program = {0};
    if (!MotionOutputOpen(&program, prefix, "gcode")) return 0;
    MotionPath path = {0};
    MachineRequirementsFrames requirements={0};
    MotionPoint cursor_mm = {0, 0};
    int success = 0;
    for (unsigned frame = 0; frame < frames; frame++) {
        if (frame) { G_Ticker(); ++gametic; }
        if (!CaptureFrame(&path, cursor_mm) || !WriteFrameSVG(prefix, frame, &path)) goto done;
        // Assumed feed is 3600 mm/min; M0 is an unbounded operator pause.
        if (!MotionWriteGCodeFrame(program.stream, &path, 3600, frame == 0, frame + 1 == frames)) goto done;
        if (!MotionQuantizePoint(&path, path.points[path.count - 1], &cursor_mm)) goto done;
        ReportMotion(&path);
    }
    if (target_fps && !CommandRequirements(program.stream,target_fps,&requirements)) goto done;
    success = 1;
done:
    MotionFree(&path);
    success=MotionOutputClose(&program,success);
    if (success && target_fps)
        success=MachineRequirementsFramesWrite(stdout,&requirements,0) && fflush(stdout)==0;
    MachineRequirementsFramesFree(&requirements);
    return success;
}

int main(int argc, char **argv)
{
    unsigned frames = 1;
    double target_fps=0;
    if (argc < 3) {
        fprintf(stderr, "Usage: %s IWAD PREFIX [--no-sprites] [--no-hud] [--no-weapon]\n"
            "  [--no-textures] [--no-sprite-textures] [--no-weapon-textures] [--hatch-pattern 0..2] [--frames 1..10000] [--target-fps N]\n", argv[0]);
        return 2;
    }
    for (int argument = 3; argument < argc; ++argument) {
        if (!strcmp(argv[argument], "--no-sprites")) R_EtchSprites(0);
        else if (!strcmp(argv[argument], "--no-hud")) R_EtchHUD(0);
        else if (!strcmp(argv[argument], "--no-weapon")) R_EtchWeapon(0);
        else if (!strcmp(argv[argument], "--no-textures")) R_EtchTextures(0);
        else if (!strcmp(argv[argument], "--no-sprite-textures")) R_EtchSpriteTextures(0);
        else if (!strcmp(argv[argument], "--no-weapon-textures")) R_EtchWeaponTextures(0);
        else if (!strcmp(argv[argument], "--target-fps")) {
            if (target_fps || argument+1>=argc ||
                !MachineRequirementsParseFPS(argv[argument+1],&target_fps)) {
                fputs("--target-fps requires one finite positive decimal value\n",stderr);
                return 2;
            }
            argument++;
        }
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
    int success = ExportFrames(argv[2], frames,target_fps);
    R_EtchResetSprites();
    if (!success) I_Error("Cannot export completed frames to %s", argv[2]);
    return 0;
}
