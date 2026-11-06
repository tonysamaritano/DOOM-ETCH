// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef R_ETCH_H
#define R_ETCH_H

#include "motion.h"

// Projected capture is optional; ordinary gameplay leaves it inactive.
int R_EtchBegin(int width, int height);
void R_EtchFrameBegin(int width, int height);
void R_EtchWallBegin(void);
void R_EtchWallColumn(int tier, int x, double top, double bottom,
                      double topstep, double bottomstep);
void R_EtchClipPlane(int x, double top, double topstep, double bottom, double bottomstep);
void R_EtchWallEnd(void);
void R_EtchTextures(int enabled);
void R_EtchSpriteTextures(int enabled);
void R_EtchWeaponTextures(int enabled);
int R_EtchTexturesActive(void);
int R_EtchSpriteTexturesEnabled(void);
int R_EtchWeaponTexturesEnabled(void);
void R_EtchHatchPattern(int pattern);
void R_EtchWallTexture(int tier, int x, const unsigned char *source,
    const unsigned char *light, const unsigned char *palette,
    double mid, double scale, double step, double center, double u, double u_end);
int R_EtchBuildMotion(MotionPath *path);
void R_EtchSprites(int enabled);
int R_EtchSpritesActive(void);
int R_EtchSpritePatch(int lump, const void *patch, size_t bytes,
    double origin_x, double origin_y, double scale_x, double scale_y,
    int first, int last, const short *top, const short *bottom,
    const unsigned char *light, const unsigned char *palette, const unsigned char *translation);
void R_EtchResetSprites(void);
void R_EtchHUD(int enabled);
void R_EtchWeapon(int enabled);
int R_EtchHUDEnabled(void);
int R_EtchWeaponActive(void);
int R_EtchHUDBegin(void);
void R_EtchHUDEnd(void);
int R_EtchHUDCapturing(void);
int R_EtchOverlayPatch(int lump, const void *patch, size_t bytes,
    double origin_x, double origin_y, double scale_x, double scale_y,
    int first, int last, const short *top, const short *bottom,
    const unsigned char *light, const unsigned char *palette, int hud);

#endif
